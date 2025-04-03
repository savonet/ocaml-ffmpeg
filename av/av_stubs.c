#include <string.h>

#define CAML_NAME_SPACE 1

#include <caml/alloc.h>
#include <caml/bigarray.h>
#include <caml/callback.h>
#include <caml/custom.h>
#include <caml/fail.h>
#include <caml/memory.h>
#include <caml/mlvalues.h>
#include <caml/printexc.h>
#include <caml/threads.h>

#ifndef Bytes_val
#define Bytes_val String_val
#endif

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/audio_fifo.h>
#include <libavutil/avstring.h>
#include <libavutil/mem.h>
#include <libavutil/opt.h>
#include <libavutil/parseutils.h>
#include <libavutil/timestamp.h>

#include "av_stubs.h"
#include "avcodec_stubs.h"
#include "avutil_stubs.h"

/**** Init ****/

value ocaml_av_init(value unit) {
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58, 9, 100)
  av_register_all();
#endif
  avformat_network_init();
  return Val_unit;
}

/**** Context ****/

typedef struct {
  int index;
  AVCodecContext *codec_context;

  // input
  int got_frame;
} stream_t;

typedef struct av_t {
  AVFormatContext *format_context;
  stream_t **streams;
  value control_message_callback;
  int is_input;
  value interrupt_cb;
  int closed;

  // input
  int end_of_file;
  int frames_pending;
  stream_t *best_audio_stream;
  stream_t *best_video_stream;
  stream_t *best_subtitle_stream;

  // output
  int header_written;
  int (*write_frame)(AVFormatContext *, AVPacket *);
  int custom_io;
} av_t;

#define Av_base_val(v) (*(av_t **)Data_custom_val(v))
static inline av_t *Av_val(value v) {
  av_t *av = Av_base_val(v);
  if (av->closed)
    Fail("Container closed!");
  return av;
}

/***** Stream handler *****/

#define StreamIndex_val(v) Int_val(Field(v, 1))

/**** Media Type ****/

static const enum AVMediaType MEDIA_TYPES[] = {
    AVMEDIA_TYPE_AUDIO, AVMEDIA_TYPE_VIDEO, AVMEDIA_TYPE_DATA,
    AVMEDIA_TYPE_SUBTITLE};
#define MEDIA_TYPES_LEN (sizeof(MEDIA_TYPES) / sizeof(enum AVMediaType))

enum AVMediaType MediaType_val(value v) { return MEDIA_TYPES[Int_val(v)]; }

static void free_stream(stream_t *stream) {
  if (!stream)
    return;

  if (stream->codec_context)
    avcodec_free_context(&stream->codec_context);

  av_free(stream);
}

static void close_av(av_t *av) {
  if (av->closed)
    return;

  caml_release_runtime_system();

  if (av->format_context) {
    if (av->streams) {
      unsigned int i;
      for (i = 0; i < av->format_context->nb_streams; i++) {
        if (av->streams[i])
          free_stream(av->streams[i]);
      }
      av_free(av->streams);
      av->streams = NULL;
    }

    if (av->format_context->iformat) {
      avformat_close_input(&av->format_context);
    } else if (av->format_context->oformat) {
      // Close the output file if needed.
      if (!av->custom_io &&
          !(av->format_context->oformat->flags & AVFMT_NOFILE))
        avio_closep(&av->format_context->pb);

      avformat_free_context(av->format_context);
      av->format_context = NULL;
    }

    av->best_audio_stream = NULL;
    av->best_video_stream = NULL;
    av->best_subtitle_stream = NULL;
  }

  caml_acquire_runtime_system();

  if (av->control_message_callback) {
    caml_remove_generational_global_root(&av->control_message_callback);
  }

  if (av->interrupt_cb != Val_none) {
    caml_remove_generational_global_root(&av->interrupt_cb);
    av->interrupt_cb = Val_none;
  }

  av->closed = 1;
}

static void finalize_av(value v) { av_free(Av_base_val(v)); }

static struct custom_operations av_ops = {
    "ocaml_av_context",       finalize_av,
    custom_compare_default,   custom_hash_default,
    custom_serialize_default, custom_deserialize_default};

AVFormatContext *ocaml_av_get_format_context(value *p_av) {
  return Av_val(*p_av)->format_context;
}

CAMLprim value ocaml_av_container_options(value unit) {
  CAMLparam0();
  CAMLlocal1(ret);
  CAMLreturn(value_of_avclass(ret, avformat_get_class()));
}

CAMLprim value ocaml_av_get_streams(value _av, value _media_type) {
  CAMLparam2(_av, _media_type);
  CAMLlocal2(list, cons);
  av_t *av = Av_val(_av);
  enum AVMediaType type = MediaType_val(_media_type);
  unsigned int i;

  List_init(list);

  for (i = 0; i < av->format_context->nb_streams; i++) {
    if (av->format_context->streams[i]->codecpar->codec_type == type)
      List_add(list, cons, Val_int(i));
  }

  CAMLreturn(list);
}

CAMLprim value ocaml_av_get_stream_codec_parameters(value _stream) {
  CAMLparam1(_stream);
  CAMLlocal2(ans, _av);
  _av = Field(_stream, 0);
  av_t *av = Av_val(_av);
  int index = StreamIndex_val(_stream);

  value_of_codec_parameters_copy(av->format_context->streams[index]->codecpar,
                                 &ans);

  CAMLreturn(ans);
}

CAMLprim value ocaml_av_get_stream_avg_frame_rate(value _stream) {
  CAMLparam1(_stream);
  CAMLlocal3(ans, ret, _av);
  _av = Field(_stream, 0);
  av_t *av = Av_val(_av);
  int index = StreamIndex_val(_stream);
  AVStream *st = av->format_context->streams[index];

  if (!st->avg_frame_rate.num)
    CAMLreturn(Val_none);

  value_of_rational(&av->format_context->streams[index]->avg_frame_rate, &ans);

  ret = caml_alloc_tuple(1);
  Store_field(ret, 0, ans);

  CAMLreturn(ret);
}

CAMLprim value ocaml_av_set_stream_avg_frame_rate(value _stream,
                                                  value _avg_frame_rate) {
  CAMLparam2(_stream, _avg_frame_rate);
  CAMLlocal1(_av);
  _av = Field(_stream, 0);
  av_t *av = Av_val(_av);
  int index = StreamIndex_val(_stream);
  AVStream *st = av->format_context->streams[index];

  if (_avg_frame_rate == Val_none) {
    st->avg_frame_rate = (AVRational){0, 1};
    CAMLreturn(Val_unit);
  }

  st->avg_frame_rate = rational_of_value(Field(_avg_frame_rate, 0));

  CAMLreturn(Val_unit);
}

CAMLprim value ocaml_av_get_stream_time_base(value _stream) {
  CAMLparam1(_stream);
  CAMLlocal2(ans, _av);
  _av = Field(_stream, 0);
  av_t *av = Av_val(_av);
  int index = StreamIndex_val(_stream);

  value_of_rational(&av->format_context->streams[index]->time_base, &ans);

  CAMLreturn(ans);
}

CAMLprim value ocaml_av_set_stream_time_base(value _stream, value _time_base) {
  CAMLparam2(_stream, _time_base);
  CAMLlocal1(_av);
  _av = Field(_stream, 0);
  av_t *av = Av_val(_av);
  int index = StreamIndex_val(_stream);

  av->format_context->streams[index]->time_base = rational_of_value(_time_base);

  CAMLreturn(Val_unit);
}

CAMLprim value ocaml_av_get_stream_frame_size(value _stream) {
  CAMLparam1(_stream);
  CAMLlocal1(_av);
  _av = Field(_stream, 0);
  av_t *av = Av_val(_av);
  int index = StreamIndex_val(_stream);

  CAMLreturn(Val_int(av->streams[index]->codec_context->frame_size));
}

CAMLprim value ocaml_av_get_stream_pixel_aspect(value _stream) {
  CAMLparam1(_stream);
  CAMLlocal3(ans, ret, _av);
  _av = Field(_stream, 0);
  av_t *av = Av_val(_av);
  int index = StreamIndex_val(_stream);
  const AVRational pixel_aspect =
      av->format_context->streams[index]->sample_aspect_ratio;

  if (pixel_aspect.num == 0)
    CAMLreturn(Val_none);

  value_of_rational(&pixel_aspect, &ans);

  ret = caml_alloc_tuple(1);
  Store_field(ret, 0, ans);

  CAMLreturn(ret);
}

value *ocaml_av_get_control_message_callback(struct AVFormatContext *ctx) {
  return &((av_t *)ctx->opaque)->control_message_callback;
}

void ocaml_av_set_control_message_callback(value *p_av,
                                           av_format_control_message c_callback,
                                           value *p_ocaml_callback) {
  av_t *av = Av_val(*p_av);

  if (!av->control_message_callback) {
    av->control_message_callback = *p_ocaml_callback;
    caml_register_generational_global_root(&av->control_message_callback);
  } else {
    caml_modify_generational_global_root(&av->control_message_callback,
                                         *p_ocaml_callback);
  }

  av->format_context->opaque = (void *)av;
  av->format_context->control_message_cb = c_callback;
}

/***** Input *****/

/***** AVIO *****/

#define BUFLEN 1024
#define MIN(a, b) ((a) < (b) ? (a) : (b))

typedef struct avio_t {
  AVFormatContext *format_context;
  AVIOContext *avio_context;
  value buffer;
  value read_cb;
  value write_cb;
  value seek_cb;
} avio_t;

#define Avio_val(v) (*(avio_t **)Data_abstract_val(v))

static int ocaml_avio_read_callback(void *private, uint8_t *buf, int buf_size) {
  value res;
  avio_t *avio = (avio_t *)private;
  int len = MIN(BUFLEN, buf_size);
  size_t exn_len;
  char *caml_exn = NULL;

  caml_acquire_runtime_system();

  res =
      caml_callback3_exn(avio->read_cb, avio->buffer, Val_int(0), Val_int(len));
  if (Is_exception_result(res)) {
    res = Extract_exception(res);

    caml_exn = caml_format_exception(res);
    av_log(avio->avio_context, AV_LOG_ERROR,
           "Error while executing OCaml read callback: %s\n", caml_exn);
    caml_stat_free(caml_exn);

    caml_release_runtime_system();

    return AVERROR_EXTERNAL;
  }

  if (Int_val(res) < 0) {
    caml_release_runtime_system();
    return Int_val(res);
  }

  memcpy(buf, String_val(avio->buffer), Int_val(res));

  caml_release_runtime_system();

  if (Int_val(res) == 0) {
    return AVERROR_EOF;
  }

  return Int_val(res);
}

#if LIBAVFORMAT_VERSION_MAJOR < 61
static int ocaml_avio_write_callback(void *private, uint8_t *buf,
                                     int buf_size) {
#else
static int ocaml_avio_write_callback(void *private, const uint8_t *buf,
                                     int buf_size) {
#endif
  value res;
  avio_t *avio = (avio_t *)private;
  int len = MIN(BUFLEN, buf_size);
  size_t exn_len;
  char *caml_exn = NULL;

  caml_acquire_runtime_system();

  memcpy(Bytes_val(avio->buffer), buf, len);

  res = caml_callback3_exn(avio->write_cb, avio->buffer, Val_int(0),
                           Val_int(len));
  if (Is_exception_result(res)) {
    res = Extract_exception(res);

    caml_exn = caml_format_exception(res);
    av_log(avio->avio_context, AV_LOG_ERROR,
           "Error while executing OCaml write callback: %s\n", caml_exn);
    caml_stat_free(caml_exn);

    caml_release_runtime_system();

    return AVERROR_EXTERNAL;
  }

  caml_release_runtime_system();

  return Int_val(res);
}

static int64_t ocaml_avio_seek_callback(void *private, int64_t offset,
                                        int whence) {
  value res;
  avio_t *avio = (avio_t *)private;
  int _whence;
  int64_t n;

  switch (whence) {
  case SEEK_SET:
    _whence = 0;
    break;
  case SEEK_CUR:
    _whence = 1;
    break;
  case SEEK_END:
    _whence = 2;
    break;
  default:
    return -1;
  }

  caml_acquire_runtime_system();

  res = caml_callback2(avio->seek_cb, Val_int(offset), Val_int(_whence));

  n = Int_val(res);

  caml_release_runtime_system();

  return n;
}

static int ocaml_av_interrupt_callback(void *private) {
  value res;
  av_t *av = (av_t *)private;
  int n;

  caml_acquire_runtime_system();
  res = caml_callback(av->interrupt_cb, Val_unit);
  n = Int_val(res);
  caml_release_runtime_system();

  return n;
};

CAMLprim value ocaml_av_create_io(value bufsize, value _read_cb,
                                  value _write_cb, value _seek_cb) {
  CAMLparam3(_read_cb, _write_cb, _seek_cb);
  CAMLlocal1(ret);

  int (*read_cb)(void *opaque, uint8_t *buf, int buf_size) = NULL;
#if LIBAVFORMAT_VERSION_MAJOR < 61
  int (*write_cb)(void *opaque, uint8_t *buf, int buf_size) = NULL;
#else
  int (*write_cb)(void *opaque, const uint8_t *buf, int buf_size) = NULL;
#endif
  int64_t (*seek_cb)(void *opaque, int64_t offset, int whence) = NULL;
  int write_flag = 0;
  unsigned char *buffer;
  int buffer_size;

  avio_t *avio = (avio_t *)av_mallocz(sizeof(avio_t));
  if (!avio)
    caml_raise_out_of_memory();

  avio->buffer = caml_alloc_string(BUFLEN);
  caml_register_generational_global_root(&avio->buffer);

  avio->read_cb = (value)NULL;
  avio->write_cb = (value)NULL;
  avio->seek_cb = (value)NULL;

  avio->format_context = avformat_alloc_context();

  if (!avio->format_context) {
    av_free(avio);
    caml_raise_out_of_memory();
  }

  buffer_size = Int_val(bufsize);
  buffer = av_malloc(buffer_size);

  if (!buffer) {
    avformat_free_context(avio->format_context);
    av_free(avio);
    caml_raise_out_of_memory();
  }

  if (_read_cb != Val_none) {
    avio->read_cb = Some_val(_read_cb);
    caml_register_generational_global_root(&avio->read_cb);
    read_cb = ocaml_avio_read_callback;
  }

  if (_write_cb != Val_none) {
    avio->write_cb = Some_val(_write_cb);
    caml_register_generational_global_root(&avio->write_cb);
    write_cb = ocaml_avio_write_callback;
    write_flag = 1;
  }

  if (_seek_cb != Val_none) {
    avio->seek_cb = Some_val(_seek_cb);
    caml_register_generational_global_root(&avio->seek_cb);
    seek_cb = ocaml_avio_seek_callback;
  }

  avio->avio_context =
      avio_alloc_context(buffer, buffer_size, write_flag, (void *)avio, read_cb,
                         write_cb, seek_cb);

  if (!avio->avio_context) {
    caml_remove_generational_global_root(&avio->buffer);

    if (avio->read_cb)
      caml_remove_generational_global_root(&avio->read_cb);

    if (avio->write_cb)
      caml_remove_generational_global_root(&avio->write_cb);

    if (avio->seek_cb)
      caml_remove_generational_global_root(&avio->seek_cb);

    av_freep(buffer);
    avformat_free_context(avio->format_context);
    av_free(avio);
    caml_raise_out_of_memory();
  }

  avio->format_context->pb = avio->avio_context;

  ret = caml_alloc(1, Abstract_tag);

  Avio_val(ret) = avio;

  CAMLreturn(ret);
}

CAMLprim value caml_av_input_io_finalise(value _avio) {
  CAMLparam1(_avio);
  avio_t *avio = Avio_val(_avio);

  // format_context is freed as part of close_av.
  av_free(avio->avio_context->buffer);
  avio_context_free(&avio->avio_context);

  caml_remove_generational_global_root(&avio->buffer);

  if (avio->read_cb)
    caml_remove_generational_global_root(&avio->read_cb);

  if (avio->write_cb)
    caml_remove_generational_global_root(&avio->write_cb);

  if (avio->seek_cb)
    caml_remove_generational_global_root(&avio->seek_cb);

  av_free(avio);

  CAMLreturn(Val_unit);
}

/***** AVInputFormat *****/

void value_of_inputFormat(avioformat_const AVInputFormat *inputFormat,
                          value *p_value) {
  if (!inputFormat)
    Fail("Empty input format");

  *p_value = caml_alloc(1, Abstract_tag);
  InputFormat_val((*p_value)) = inputFormat;
}

CAMLprim value ocaml_av_find_input_format(value _short_name) {
  CAMLparam1(_short_name);
  CAMLlocal1(ret);
  char *short_name =
      av_strndup(String_val(_short_name), caml_string_length(_short_name));

  if (!short_name)
    caml_raise_out_of_memory();

  caml_release_runtime_system();
  avioformat_const AVInputFormat *format = av_find_input_format(short_name);
  caml_acquire_runtime_system();

  av_free(short_name);

  if (!format)
    caml_raise_not_found();

  value_of_inputFormat(format, &ret);

  CAMLreturn(ret);
}

CAMLprim value ocaml_av_input_format_get_name(value _format) {
  CAMLparam1(_format);
  const char *n = InputFormat_val(_format)->name;
  CAMLreturn(caml_copy_string(n ? n : ""));
}

CAMLprim value ocaml_av_input_format_get_long_name(value _format) {
  CAMLparam1(_format);
  const char *n = InputFormat_val(_format)->long_name;
  CAMLreturn(caml_copy_string(n ? n : ""));
}

static av_t *open_input(char *url, avioformat_const AVInputFormat *format,
                        AVFormatContext *format_context, value _interrupt,
                        AVDictionary **options) {
  int err;

  av_t *av = (av_t *)av_mallocz(sizeof(av_t));
  if (!av) {
    if (url)
      av_free(url);
    caml_raise_out_of_memory();
  }

  if (format_context) {
    av->format_context = format_context;
  } else {
    av->format_context = avformat_alloc_context();
  }

  if (!av->format_context) {
    if (url)
      av_free(url);
    av_free(av);
    caml_raise_out_of_memory();
  }

  av->closed = 0;
  av->is_input = 1;
  av->frames_pending = 0;
  av->streams = NULL;

  if (_interrupt != Val_none) {
    av->interrupt_cb = Some_val(_interrupt);
    caml_register_generational_global_root(&av->interrupt_cb);
    av->format_context->interrupt_callback.callback =
        ocaml_av_interrupt_callback;
    av->format_context->interrupt_callback.opaque = (void *)av;
  } else {
    av->interrupt_cb = Val_none;
  }

  caml_release_runtime_system();
  err = avformat_open_input(&av->format_context, url, format, options);
  caml_acquire_runtime_system();

  if (err < 0) {
    if (av->interrupt_cb != Val_none) {
      caml_remove_generational_global_root(&av->interrupt_cb);
      av->interrupt_cb = Val_none;
    }
    av_free(av);
    if (url)
      av_free(url);
    av_dict_free(options);
    ocaml_avutil_raise_error(err);
  }

  // retrieve stream information
  caml_release_runtime_system();
  err = avformat_find_stream_info(av->format_context, NULL);
  caml_acquire_runtime_system();

  if (err < 0) {
    avformat_close_input(&av->format_context);
    if (av->interrupt_cb != Val_none) {
      caml_remove_generational_global_root(&av->interrupt_cb);
      av->interrupt_cb = Val_none;
    }
    av_free(av);
    if (url)
      av_free(url);
    av_dict_free(options);
    ocaml_avutil_raise_error(err);
  }

  return av;
}

CAMLprim value ocaml_av_open_input(value _url, value _format, value _interrupt,
                                   value _opts) {
  CAMLparam4(_url, _format, _interrupt, _opts);
  CAMLlocal3(ret, ans, unused);
  char *url = NULL;
  avioformat_const AVInputFormat *format = NULL;
  int ulen = caml_string_length(_url);
  AVDictionary *options = NULL;
  char *key, *val;
  int len = Wosize_val(_opts);
  int i, err, count;

  for (i = 0; i < len; i++) {
    // Dictionaries copy key/values by default!
    key = (char *)Bytes_val(Field(Field(_opts, i), 0));
    val = (char *)Bytes_val(Field(Field(_opts, i), 1));
    err = av_dict_set(&options, key, val, 0);
    if (err < 0) {
      av_dict_free(&options);
      ocaml_avutil_raise_error(err);
    }
  }

  if (ulen > 0)
    url = av_strndup(String_val(_url), ulen);

  if (_format != Val_none)
    format = InputFormat_val(Some_val(_format));

  if (format == NULL && url == NULL) {
    av_dict_free(&options);
    Fail("At least one format or url must be provided!");
  }

  // open input url
  av_t *av = open_input(url, format, NULL, _interrupt, &options);

  if (url)
    av_free(url);

  // Return unused keys
  count = av_dict_count(options);

  unused = caml_alloc_tuple(count);
  AVDictionaryEntry *entry = NULL;
  for (i = 0; i < count; i++) {
    entry = av_dict_get(options, "", entry, AV_DICT_IGNORE_SUFFIX);
    Store_field(unused, i, caml_copy_string(entry->key));
  }

  av_dict_free(&options);

  // allocate format context
  ans = caml_alloc_custom(&av_ops, sizeof(av_t *), 0, 1);
  Av_base_val(ans) = av;

  ret = caml_alloc_tuple(2);
  Store_field(ret, 0, ans);
  Store_field(ret, 1, unused);

  CAMLreturn(ret);
}

CAMLprim value ocaml_av_open_input_stream(value _avio, value _format,
                                          value _opts) {
  CAMLparam3(_avio, _format, _opts);
  CAMLlocal3(ret, ans, unused);
  avio_t *avio = Avio_val(_avio);
  avioformat_const AVInputFormat *format = NULL;
  AVDictionary *options = NULL;
  char *key, *val;
  int len = Wosize_val(_opts);
  int i, err, count;

  for (i = 0; i < len; i++) {
    // Dictionaries copy key/values by default!
    key = (char *)Bytes_val(Field(Field(_opts, i), 0));
    val = (char *)Bytes_val(Field(Field(_opts, i), 1));
    err = av_dict_set(&options, key, val, 0);
    if (err < 0) {
      av_dict_free(&options);
      ocaml_avutil_raise_error(err);
    }
  }

  if (_format != Val_none)
    format = InputFormat_val(Some_val(_format));

  // open input format
  av_t *av = open_input(NULL, format, avio->format_context, Val_none, &options);

  // Return unused keys
  count = av_dict_count(options);

  unused = caml_alloc_tuple(count);
  AVDictionaryEntry *entry = NULL;
  for (i = 0; i < count; i++) {
    entry = av_dict_get(options, "", entry, AV_DICT_IGNORE_SUFFIX);
    Store_field(unused, i, caml_copy_string(entry->key));
  }

  av_dict_free(&options);

  // allocate format context
  ans = caml_alloc_custom(&av_ops, sizeof(av_t *), 0, 1);
  Av_base_val(ans) = av;

  ret = caml_alloc_tuple(2);
  Store_field(ret, 0, ans);
  Store_field(ret, 1, unused);

  CAMLreturn(ret);
}

CAMLprim value ocaml_av_input_obj(value _av) {
  CAMLparam1(_av);
  CAMLlocal1(ret);
  CAMLreturn(value_of_avobj(ret, Av_val(_av)->format_context));
}

CAMLprim value ocaml_av_get_metadata(value _av, value _stream_index) {
  CAMLparam1(_av);
  CAMLlocal3(pair, cons, list);
  av_t *av = Av_val(_av);
  int index = Int_val(_stream_index);
  AVDictionary *metadata = av->format_context->metadata;
  AVDictionaryEntry *tag = NULL;

  if (index >= 0) {
    metadata = av->format_context->streams[index]->metadata;
  }

  List_init(list);

  while ((tag = av_dict_get(metadata, "", tag, AV_DICT_IGNORE_SUFFIX))) {
    pair = caml_alloc_tuple(2);
    Store_field(pair, 0, caml_copy_string(tag->key));
    Store_field(pair, 1, caml_copy_string(tag->value));

    List_add(list, cons, pair);
  }

  CAMLreturn(list);
}

CAMLprim value ocaml_av_get_duration(value _av, value _stream_index,
                                     value _time_format) {
  CAMLparam2(_av, _time_format);
  CAMLlocal1(ans);
  av_t *av = Av_val(_av);
  int index = Int_val(_stream_index);

  if (!av->format_context)
    Fail("Failed to get closed input duration");

  int64_t duration = av->format_context->duration;
  int64_t num = 1;
  int64_t den = AV_TIME_BASE;

  if (index >= 0) {
    duration = av->format_context->streams[index]->duration;
    num = (int64_t)av->format_context->streams[index]->time_base.num;
    den = (int64_t)av->format_context->streams[index]->time_base.den;
  }

  int64_t second_fractions = second_fractions_of_time_format(_time_format);

  ans = caml_copy_int64((duration * second_fractions * num) / den);

  CAMLreturn(ans);
}

static stream_t **allocate_input_context(av_t *av) {
  if (!av->format_context)
    Fail("Failed to read closed input");

  // Allocate streams context array
  av->streams = (stream_t **)av_calloc(av->format_context->nb_streams,
                                       sizeof(stream_t *));
  if (!av->streams)
    caml_raise_out_of_memory();

  return av->streams;
}

static stream_t *allocate_stream_context(av_t *av, int index,
                                         const AVCodec *codec) {
  if (codec) {
    enum AVMediaType type = codec->type;

    if (type != AVMEDIA_TYPE_AUDIO && type != AVMEDIA_TYPE_VIDEO &&
        type != AVMEDIA_TYPE_SUBTITLE)
      Fail("Failed to allocate stream %d of media type %s", index,
           av_get_media_type_string(type));
  }

  stream_t *stream = (stream_t *)av_mallocz(sizeof(stream_t));
  if (!stream)
    caml_raise_out_of_memory();

  stream->index = index;
  av->streams[index] = stream;

  if (!codec)
    return stream;

  stream->codec_context = avcodec_alloc_context3(codec);

  if (!stream->codec_context) {
    caml_raise_out_of_memory();
  }

  return stream;
}

static stream_t *open_stream_index(av_t *av, int index, const AVCodec *dec) {
  int err;

  if (!av->format_context)
    Fail("Failed to open stream %d of closed input", index);

  if (index < 0 || index >= av->format_context->nb_streams)
    Fail("Failed to open stream %d : index out of bounds", index);

  if (!av->streams && !allocate_input_context(av))
    caml_raise_out_of_memory();

  // find decoder for the stream
  AVCodecParameters *dec_param = av->format_context->streams[index]->codecpar;
  if (!dec) {
    caml_release_runtime_system();
    dec = avcodec_find_decoder(dec_param->codec_id);
    caml_acquire_runtime_system();
  }

  if (!dec)
    ocaml_avutil_raise_error(AVERROR_DECODER_NOT_FOUND);

  stream_t *stream = allocate_stream_context(av, index, dec);
  if (!stream)
    caml_raise_out_of_memory();

  // initialize the stream parameters with demuxer information
  err = avcodec_parameters_to_context(stream->codec_context, dec_param);

  if (err < 0) {
    av_free(stream);
    ocaml_avutil_raise_error(err);
  }

  // Open the decoder
  caml_release_runtime_system();
  err = avcodec_open2(stream->codec_context, dec, NULL);
  caml_acquire_runtime_system();

  if (err < 0) {
    av_free(stream);
    ocaml_avutil_raise_error(err);
  }

  return stream;
}

CAMLprim value ocaml_av_find_best_stream(value _av, value _media_type) {
  CAMLparam2(_av, _media_type);
  av_t *av = Av_val(_av);
  enum AVMediaType type = MediaType_val(_media_type);

  caml_release_runtime_system();
  int index = av_find_best_stream(av->format_context, type, -1, -1, NULL, 0);
  caml_acquire_runtime_system();

  if (index < 0)
    ocaml_avutil_raise_error(AVERROR_STREAM_NOT_FOUND);

  CAMLreturn(Val_int(index));
}

static int decode_packet(av_t *av, stream_t *stream, AVPacket *packet,
                         AVFrame *frame) {
  AVCodecContext *dec = stream->codec_context;
  int ret = 0;

  caml_release_runtime_system();

  if (dec->codec_type == AVMEDIA_TYPE_AUDIO ||
      dec->codec_type == AVMEDIA_TYPE_VIDEO) {

    // Assumption: each time this function is called with `frames_pending ==
    // 0`, a fresh packet is also provided and no packet otherwise.
    if (!av->frames_pending) {
      ret = avcodec_send_packet(dec, packet);

      if (ret < 0) {
        caml_acquire_runtime_system();
        return ret;
      }

      av_packet_unref(packet);
      av->frames_pending = 1;
    }

    // decode frame
    ret = avcodec_receive_frame(dec, frame);

    if (ret == AVERROR(EAGAIN))
      av->frames_pending = 0;
  } else if (dec->codec_type == AVMEDIA_TYPE_SUBTITLE) {
    ret = avcodec_decode_subtitle2(dec, (AVSubtitle *)frame, &stream->got_frame,
                                   packet);
    if (ret >= 0) {
      av_packet_unref(packet);
      caml_acquire_runtime_system();
      return ret;
    }
  }

  av_packet_unref(packet);

  caml_acquire_runtime_system();

  stream->got_frame = 1;

  return ret;
}

static int read_packet(av_t *av, AVPacket *packet) {
  int ret;

  caml_release_runtime_system();

  ret = av_read_frame(av->format_context, packet);

  if (ret == AVERROR_EOF) {
    packet->data = NULL;
    packet->size = 0;
    av->end_of_file = 1;
    caml_acquire_runtime_system();
    return 0;
  }

  caml_acquire_runtime_system();

  return ret;
}

CAMLprim value ocaml_av_read_input(value _packet, value _frame, value _av) {
  CAMLparam3(_packet, _frame, _av);
  CAMLlocal3(ans, decoded_content, frame_value);
  av_t *av = Av_val(_av);
  AVFrame *frame;
  int i, ret, err, frame_kind, skip;
  value _dec;
  const AVCodec *dec = NULL;

  if (!av->streams && !allocate_input_context(av))
    caml_raise_out_of_memory();

  AVPacket *packet = av_packet_alloc();
  if (!packet) {
    caml_raise_out_of_memory();
  }
  packet->data = NULL;
  packet->size = 0;

  stream_t **streams = av->streams;
  unsigned int nb_streams = av->format_context->nb_streams;
  stream_t *stream = NULL;

  do {
    if (!av->end_of_file && !av->frames_pending) {
      // Don't use ret here as it is the conditional for the
      // loop.
      err = read_packet(av, packet);
      if (err < 0) {
        av_packet_free(&packet);
        ocaml_avutil_raise_error(err);
      }

      if (av->end_of_file)
        continue;

      skip = 1;
      for (i = 0; i < Wosize_val(_packet); i++)
        if (Int_val(Field(Field(_packet, i), 0)) == packet->stream_index) {
          skip = 0;
        }

      for (i = 0; i < Wosize_val(_frame); i++)
        if (Int_val(Field(Field(_frame, i), 0)) == packet->stream_index) {
          _dec = Field(Field(_frame, i), 1);

          if (_dec != Val_none) {
            dec = AvCodec_val(Some_val(_dec));
          }

          if ((stream = streams[packet->stream_index]) == NULL)
            stream = open_stream_index(av, packet->stream_index, dec);

          skip = 0;
        }

      if (skip) {
        av_packet_unref(packet);
        continue;
      }

      for (i = 0; i < Wosize_val(_packet); i++) {
        if (Int_val(Field(Field(_packet, i), 0)) == packet->stream_index) {
          decoded_content = caml_alloc_tuple(2);
          Store_field(decoded_content, 0, Val_int(packet->stream_index));
          Store_field(decoded_content, 1, value_of_ffmpeg_packet(packet));

          ans = caml_alloc_tuple(2);

          switch (Field(Field(_packet, i), 1)) {
          case PVV_Audio:
            Store_field(ans, 0, PVV_Audio_packet);
            break;
          case PVV_Video:
            Store_field(ans, 0, PVV_Video_packet);
            break;
          case PVV_Data:
            Store_field(ans, 0, PVV_Data_packet);
            break;
          default:
            Store_field(ans, 0, PVV_Subtitle_packet);
            break;
          }

          Store_field(ans, 1, decoded_content);

          CAMLreturn(ans);
        }
      }
    } else {
      for (i = 0; i < nb_streams; i++) {
        if ((stream = streams[i]) && stream->got_frame)
          break;
      }

      if (i == nb_streams)
        ocaml_avutil_raise_error(AVERROR_EOF);
    }

    skip = 1;
    for (i = 0; i < Wosize_val(_frame); i++) {
      if (Int_val(Field(Field(_frame, i), 0)) == stream->index) {
        skip = 0;
      }
    }

    if (skip) {
      av_packet_unref(packet);
      continue;
    }

    // Assign OCaml values right away to account for potential exceptions
    // raised below.
    if (stream->codec_context->codec_type == AVMEDIA_TYPE_SUBTITLE) {
      frame = (AVFrame *)av_mallocz(sizeof(AVSubtitle));

      if (!frame) {
        av_packet_free(&packet);
        caml_raise_out_of_memory();
      }

      frame_kind = PVV_Subtitle_frame;
      frame_value = value_of_subtitle((AVSubtitle *)frame);
    } else {
      frame = av_frame_alloc();

      if (!frame) {
        av_packet_free(&packet);
        caml_raise_out_of_memory();
      }

      if (stream->codec_context->codec_type == AVMEDIA_TYPE_AUDIO)
        frame_kind = PVV_Audio_frame;
      else
        frame_kind = PVV_Video_frame;

      frame_value = value_of_frame(frame);
    }

    ret = decode_packet(av, stream, packet, frame);

    if (ret < 0 && ret != AVERROR(EAGAIN)) {
      av_packet_free(&packet);
      ocaml_avutil_raise_error(ret);
    }

    av_packet_unref(packet);
  } while (ret == AVERROR(EAGAIN));

  av_packet_free(&packet);

  decoded_content = caml_alloc_tuple(2);
  Store_field(decoded_content, 0, Val_int(stream->index));
  Store_field(decoded_content, 1, frame_value);

  ans = caml_alloc_tuple(2);
  Store_field(ans, 0, frame_kind);
  Store_field(ans, 1, decoded_content);

  CAMLreturn(ans);
}

static const int seek_flags[] = {AVSEEK_FLAG_BACKWARD, AVSEEK_FLAG_BYTE,
                                 AVSEEK_FLAG_ANY, AVSEEK_FLAG_FRAME};

static int seek_flags_val(value v) { return seek_flags[Int_val(v)]; }

CAMLprim value ocaml_av_seek_native(value _flags, value _stream, value _min_ts,
                                    value _max_ts, value _time_format,
                                    value _timestamp, value _av) {
  CAMLparam5(_flags, _stream, _min_ts, _max_ts, _time_format);
  CAMLxparam2(_timestamp, _av);

  av_t *av = Av_val(_av);
  int index = -1;
  int64_t min_ts = INT64_MIN;
  int64_t max_ts = INT64_MAX;
  int64_t timestamp = Int64_val(_timestamp);
  int64_t second_fractions = second_fractions_of_time_format(_time_format);
  int64_t num = AV_TIME_BASE;
  int64_t den = 1;
  int flags = 0;
  int ret, i;

  if (!av->format_context)
    Fail("Failed to seek closed input");

  if (_stream != Val_none) {
    index = StreamIndex_val(Field(_stream, 0));
  }

  if (index >= 0) {
    num = (int64_t)av->format_context->streams[index]->time_base.den;
    den = (int64_t)av->format_context->streams[index]->time_base.num;
  }

  timestamp = (timestamp * num) / (den * second_fractions);

  if (_min_ts != Val_none) {
    min_ts = (Int64_val(Field(_min_ts, 0)) * num) / (den * second_fractions);
  }

  if (_max_ts != Val_none) {
    max_ts = (Int64_val(Field(_max_ts, 0)) * num) / (den * second_fractions);
  }

  for (i = 0; i < Wosize_val(_flags); i++)
    flags |= seek_flags_val(Field(_flags, i));

  caml_release_runtime_system();

  ret = avformat_seek_file(av->format_context, index, min_ts, timestamp, max_ts,
                           flags);

  caml_acquire_runtime_system();

  if (ret < 0)
    ocaml_avutil_raise_error(ret);

  CAMLreturn(Val_unit);
}

CAMLprim value ocaml_av_seek_bytecode(value *argv, int argn) {
  return ocaml_av_seek_native(argv[0], argv[1], argv[2], argv[3], argv[4],
                              argv[5], argv[6]);
}

/***** Output *****/

/***** AVOutputFormat *****/

value value_of_outputFormat(avioformat_const AVOutputFormat *outputFormat) {
  value v;
  if (!outputFormat)
    Fail("Empty output format");

  v = caml_alloc(1, Abstract_tag);
  OutputFormat_val(v) = outputFormat;

  return v;
}

CAMLprim value ocaml_av_output_format_guess(value _short_name, value _filename,
                                            value _mime) {
  CAMLparam3(_short_name, _filename, _mime);
  CAMLlocal1(ans);
  char *short_name = NULL;
  char *filename = NULL;
  char *mime = NULL;
  avioformat_const AVOutputFormat *guessed;

  if (caml_string_length(_short_name) > 0) {
    short_name =
        av_strndup(String_val(_short_name), caml_string_length(_short_name));
    if (!short_name)
      caml_raise_out_of_memory();
  };

  if (caml_string_length(_filename) > 0) {
    filename = av_strndup(String_val(_filename), caml_string_length(_filename));
    if (!filename) {
      if (short_name)
        av_free(short_name);
      caml_raise_out_of_memory();
    }
  }

  if (caml_string_length(_mime) > 0) {
    mime = av_strndup(String_val(_mime), caml_string_length(_mime));
    if (!mime) {
      if (short_name)
        av_free(short_name);
      if (filename)
        av_free(filename);
      caml_raise_out_of_memory();
    }
  }

  caml_release_runtime_system();
  guessed = av_guess_format(short_name, filename, mime);
  caml_acquire_runtime_system();

  if (short_name)
    av_free(short_name);
  if (filename)
    av_free(filename);
  if (mime)
    av_free(mime);

  if (!guessed)
    CAMLreturn(Val_none);

  ans = caml_alloc_tuple(1);
  Store_field(ans, 0, value_of_outputFormat(guessed));

  CAMLreturn(ans);
}

CAMLprim value ocaml_av_output_format_get_name(value _format) {
  CAMLparam1(_format);
  const char *n = OutputFormat_val(_format)->name;
  CAMLreturn(caml_copy_string(n ? n : ""));
}

CAMLprim value ocaml_av_output_format_get_long_name(value _format) {
  CAMLparam1(_format);
  const char *n = OutputFormat_val(_format)->long_name;
  CAMLreturn(caml_copy_string(n ? n : ""));
}

CAMLprim value ocaml_av_output_format_get_audio_codec_id(value _output_format) {
  CAMLparam1(_output_format);
  CAMLreturn(Val_AudioCodecID(OutputFormat_val(_output_format)->audio_codec));
}

CAMLprim value ocaml_av_output_format_get_video_codec_id(value _output_format) {
  CAMLparam1(_output_format);
  CAMLreturn(Val_VideoCodecID(OutputFormat_val(_output_format)->video_codec));
}

CAMLprim value
ocaml_av_output_format_get_subtitle_codec_id(value _output_format) {
  CAMLparam1(_output_format);
  CAMLreturn(
      Val_SubtitleCodecID(OutputFormat_val(_output_format)->subtitle_codec));
}

static av_t *open_output(avioformat_const AVOutputFormat *format,
                         char *file_name, AVIOContext *avio_context,
                         value _interrupt, int interleaved,
                         AVDictionary **options) {
  int ret;
  AVIOInterruptCB interrupt_cb = {ocaml_av_interrupt_callback, NULL};
  AVIOInterruptCB *interrupt_cb_ptr = NULL;
  av_t *av = (av_t *)av_mallocz(sizeof(av_t));

  if (!av) {
    if (file_name)
      av_free(file_name);
    av_dict_free(options);
    caml_raise_out_of_memory();
  }

  av->closed = 0;

  if (interleaved) {
    av->write_frame = &av_interleaved_write_frame;
  } else {
    av->write_frame = &av_write_frame;
  }

  if (_interrupt != Val_none) {
    av->interrupt_cb = Some_val(_interrupt);
    caml_register_generational_global_root(&av->interrupt_cb);
    interrupt_cb.opaque = (void *)av;
    interrupt_cb_ptr = &interrupt_cb;
  } else {
    av->interrupt_cb = Val_none;
  }

  ret = avformat_alloc_output_context2(&av->format_context, format, NULL,
                                       file_name);

  if (ret < 0) {
    if (file_name)
      av_free(file_name);
    av_dict_free(options);
    av_free(av);

    ocaml_avutil_raise_error(ret);
  }

  ret = av_opt_set_dict(av->format_context, options);

  if (ret < 0) {
    if (av->interrupt_cb != Val_none) {
      caml_remove_generational_global_root(&av->interrupt_cb);
      av->interrupt_cb = Val_none;
    }

    av_free(av);
    if (file_name)
      av_free(file_name);

    av_dict_free(options);
    av_free(av);

    ocaml_avutil_raise_error(ret);
  }

  if (av->format_context->priv_data) {
    ret = av_opt_set_dict(av->format_context->priv_data, options);

    if (ret < 0) {
      av_free(av);
      if (file_name)
        av_free(file_name);

      av_dict_free(options);
      ocaml_avutil_raise_error(ret);
    }
  }

  // open the output file, if needed
  if (avio_context) {
    if (av->format_context->oformat->flags & AVFMT_NOFILE) {
      av_free(av);
      if (file_name)
        av_free(file_name);
      av_dict_free(options);

      av_dict_free(options);
      Fail("Cannot set custom I/O on this format!");
    }

    av->format_context->pb = avio_context;
    av->custom_io = 1;
  } else {
    if (!(av->format_context->oformat->flags & AVFMT_NOFILE)) {
      caml_release_runtime_system();
      int err = avio_open2(&av->format_context->pb, file_name, AVIO_FLAG_WRITE,
                           interrupt_cb_ptr, options);
      caml_acquire_runtime_system();

      if (err < 0) {
        if (av->interrupt_cb != Val_none) {
          caml_remove_generational_global_root(&av->interrupt_cb);
          av->interrupt_cb = Val_none;
        }

        av_free(av);
        if (file_name)
          av_free(file_name);
        av_dict_free(options);

        av_dict_free(options);
        ocaml_avutil_raise_error(err);
      }

      av->custom_io = 0;
    }
  }

  if (file_name)
    av_free(file_name);

  return av;
}

CAMLprim value ocaml_av_open_output(value _interrupt, value _format,
                                    value _filename, value _interleaved,
                                    value _opts) {
  CAMLparam3(_interrupt, _filename, _opts);
  CAMLlocal3(ans, ret, unused);
  char *filename =
      av_strndup(String_val(_filename), caml_string_length(_filename));
  avioformat_const AVOutputFormat *format = NULL;
  AVDictionary *options = NULL;
  char *key, *val;
  int len = Wosize_val(_opts);
  int i, err, count;

  for (i = 0; i < len; i++) {
    // Dictionaries copy key/values by default!
    key = (char *)Bytes_val(Field(Field(_opts, i), 0));
    val = (char *)Bytes_val(Field(Field(_opts, i), 1));
    err = av_dict_set(&options, key, val, 0);
    if (err < 0) {
      av_dict_free(&options);
      ocaml_avutil_raise_error(err);
    }
  }

  if (_format != Val_none)
    format = OutputFormat_val(Some_val(_format));

  // open output file
  av_t *av = open_output(format, filename, NULL, _interrupt,
                         Bool_val(_interleaved), &options);

  // Return unused keys
  count = av_dict_count(options);

  unused = caml_alloc_tuple(count);
  AVDictionaryEntry *entry = NULL;
  for (i = 0; i < count; i++) {
    entry = av_dict_get(options, "", entry, AV_DICT_IGNORE_SUFFIX);
    Store_field(unused, i, caml_copy_string(entry->key));
  }

  av_dict_free(&options);

  // allocate format context
  ans = caml_alloc_custom(&av_ops, sizeof(av_t *), 0, 1);
  Av_base_val(ans) = av;

  ret = caml_alloc_tuple(2);
  Store_field(ret, 0, ans);
  Store_field(ret, 1, unused);

  CAMLreturn(ret);
}

CAMLprim value ocaml_av_open_output_format(value _format, value _interleaved,
                                           value _opts) {
  CAMLparam2(_format, _opts);
  CAMLlocal3(ans, ret, unused);
  AVDictionary *options = NULL;
  char *key, *val;
  int len = Wosize_val(_opts);
  int i, err, count;

  for (i = 0; i < len; i++) {
    // Dictionaries copy key/values by default!
    key = (char *)Bytes_val(Field(Field(_opts, i), 0));
    val = (char *)Bytes_val(Field(Field(_opts, i), 1));
    err = av_dict_set(&options, key, val, 0);
    if (err < 0) {
      av_dict_free(&options);
      ocaml_avutil_raise_error(err);
    }
  }

  avioformat_const AVOutputFormat *format = OutputFormat_val(_format);

  // open output format
  av_t *av = open_output(format, NULL, NULL, Val_none, Bool_val(_interleaved),
                         &options);

  // Return unused keys
  count = av_dict_count(options);

  unused = caml_alloc_tuple(count);
  AVDictionaryEntry *entry = NULL;
  for (i = 0; i < count; i++) {
    entry = av_dict_get(options, "", entry, AV_DICT_IGNORE_SUFFIX);
    Store_field(unused, i, caml_copy_string(entry->key));
  }

  av_dict_free(&options);

  // allocate format context
  ans = caml_alloc_custom(&av_ops, sizeof(av_t *), 0, 1);
  Av_base_val(ans) = av;

  ret = caml_alloc_tuple(2);
  Store_field(ret, 0, ans);
  Store_field(ret, 1, unused);

  CAMLreturn(ret);
}

CAMLprim value ocaml_av_open_output_stream(value _format, value _avio,
                                           value _interleaved, value _opts) {
  CAMLparam3(_format, _avio, _opts);
  CAMLlocal3(ans, ret, unused);
  avioformat_const AVOutputFormat *format = OutputFormat_val(_format);
  avio_t *avio = Avio_val(_avio);
  AVDictionary *options = NULL;
  char *key, *val;
  int len = Wosize_val(_opts);
  int i, err, count;

  for (i = 0; i < len; i++) {
    // Dictionaries copy key/values by default!
    key = (char *)Bytes_val(Field(Field(_opts, i), 0));
    val = (char *)Bytes_val(Field(Field(_opts, i), 1));
    err = av_dict_set(&options, key, val, 0);
    if (err < 0) {
      av_dict_free(&options);
      ocaml_avutil_raise_error(err);
    }
  }

  // open output format
  av_t *av = open_output(format, NULL, avio->avio_context, Val_none,
                         Bool_val(_interleaved), &options);

  // Return unused keys
  count = av_dict_count(options);

  unused = caml_alloc_tuple(count);
  AVDictionaryEntry *entry = NULL;
  for (i = 0; i < count; i++) {
    entry = av_dict_get(options, "", entry, AV_DICT_IGNORE_SUFFIX);
    Store_field(unused, i, caml_copy_string(entry->key));
  }

  av_dict_free(&options);

  // allocate format context
  ans = caml_alloc_custom(&av_ops, sizeof(av_t *), 0, 1);
  Av_base_val(ans) = av;

  ret = caml_alloc_tuple(2);
  Store_field(ret, 0, ans);
  Store_field(ret, 1, unused);

  CAMLreturn(ret);
}

CAMLprim value ocaml_av_header_written(value _av) {
  CAMLparam1(_av);
  av_t *av = Av_val(_av);

  CAMLreturn(Val_bool(av->header_written));
}

CAMLprim value ocaml_av_set_metadata(value _av, value _stream_index,
                                     value _tags) {
  CAMLparam2(_av, _tags);
  CAMLlocal1(pair);
  av_t *av = Av_val(_av);
  int index = Int_val(_stream_index);
  AVDictionary *metadata = NULL;

  if (!av->format_context)
    Fail("Failed to set metadata to closed output");
  if (av->header_written)
    Fail("Failed to set metadata : header already written");

  int i, ret, len = Wosize_val(_tags);

  av_dict_free(&metadata);
  for (i = 0; i < len; i++) {

    pair = Field(_tags, i);

    ret = av_dict_set(&metadata, String_val(Field(pair, 0)),
                      String_val(Field(pair, 1)), 0);
    if (ret < 0)
      ocaml_avutil_raise_error(ret);
  }

  if (index < 0) {
    av->format_context->metadata = metadata;
  } else {
    av->format_context->streams[index]->metadata = metadata;
  }

  CAMLreturn(Val_unit);
}

static stream_t *new_stream(av_t *av, const AVCodec *codec) {
  if (!av->format_context)
    Fail("Failed to add stream to closed output");
  if (av->header_written)
    Fail("Failed to create new stream : header already written");

  // Allocate streams array
  size_t streams_size =
      sizeof(stream_t *) * (av->format_context->nb_streams + 1);
  stream_t **streams = (stream_t **)realloc(av->streams, streams_size);
  if (!streams)
    caml_raise_out_of_memory();

  streams[av->format_context->nb_streams] = NULL;
  av->streams = streams;

  stream_t *stream =
      allocate_stream_context(av, av->format_context->nb_streams, codec);

  if (!stream)
    caml_raise_out_of_memory();

  AVStream *avstream = avformat_new_stream(av->format_context, codec);
  if (!avstream) {
    free_stream(stream);
    caml_raise_out_of_memory();
  }

  avstream->id = av->format_context->nb_streams - 1;

  return stream;
}

static void init_stream_encoder(AVBufferRef *device_ctx, AVBufferRef *frame_ctx,
                                av_t *av, stream_t *stream,
                                AVDictionary **options) {
  AVCodecContext *enc_ctx = stream->codec_context;
  int ret;

  // Some formats want stream headers to be separate.
  if (av->format_context->oformat->flags & AVFMT_GLOBALHEADER)
    enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

  if (device_ctx) {
    enc_ctx->hw_device_ctx = av_buffer_ref(device_ctx);
    if (!enc_ctx->hw_device_ctx) {
      av_dict_free(options);
      caml_raise_out_of_memory();
    }
  }

  if (frame_ctx) {
    enc_ctx->hw_frames_ctx = av_buffer_ref(frame_ctx);
    if (!enc_ctx->hw_frames_ctx) {
      av_dict_free(options);
      caml_raise_out_of_memory();
    }
  }

  caml_release_runtime_system();
  ret = avcodec_open2(enc_ctx, enc_ctx->codec, options);
  caml_acquire_runtime_system();

  if (ret < 0) {
    av_dict_free(options);
    ocaml_avutil_raise_error(ret);
  }

  AVStream *avstream = av->format_context->streams[stream->index];
  avstream->time_base = enc_ctx->time_base;

  ret = avcodec_parameters_from_context(avstream->codecpar, enc_ctx);

  if (ret < 0) {
    av_dict_free(options);
    ocaml_avutil_raise_error(ret);
  }
}

static stream_t *new_audio_stream(av_t *av, enum AVSampleFormat sample_fmt,
                                  AVChannelLayout *channel_layout,
                                  const AVCodec *codec,
                                  AVDictionary **options) {
  stream_t *stream = new_stream(av, codec);
  int ret;

  AVCodecContext *enc_ctx = stream->codec_context;

  enc_ctx->sample_fmt = sample_fmt;
  ret = av_channel_layout_copy(&enc_ctx->ch_layout, channel_layout);

  if (ret < 0) {
    free_stream(stream);
    ocaml_avutil_raise_error(ret);
  }

  init_stream_encoder(NULL, NULL, av, stream, options);

  return stream;
}

CAMLprim value ocaml_av_new_uninitialized_stream_copy(value _av) {
  CAMLparam1(_av);
  av_t *av = Av_val(_av);

  stream_t *stream = new_stream(av, NULL);

  CAMLreturn(Val_int(stream->index));
}

CAMLprim value ocaml_av_initialize_stream_copy(value _av, value _stream_index,
                                               value _params) {
  CAMLparam2(_av, _params);
  av_t *av = Av_val(_av);

  AVStream *avstream = av->format_context->streams[Int_val(_stream_index)];
  AVCodecParameters *params = CodecParameters_val(_params);

  int err = avcodec_parameters_copy(avstream->codecpar, params);
  if (err < 0)
    ocaml_avutil_raise_error(err);

  avstream->codecpar->codec_tag = 0;

  CAMLreturn(Val_unit);
}

CAMLprim value ocaml_av_new_audio_stream(value _av, value _sample_fmt,
                                         value _codec, value _channel_layout,
                                         value _opts) {
  CAMLparam2(_av, _opts);
  CAMLlocal2(ans, unused);
  const AVCodec *codec = AvCodec_val(_codec);

  AVDictionary *options = NULL;
  char *key, *val;
  int len = Wosize_val(_opts);
  int i, err, count;

  for (i = 0; i < len; i++) {
    // Dictionaries copy key/values by default!
    key = (char *)Bytes_val(Field(Field(_opts, i), 0));
    val = (char *)Bytes_val(Field(Field(_opts, i), 1));
    err = av_dict_set(&options, key, val, 0);
    if (err < 0) {
      av_dict_free(&options);
      ocaml_avutil_raise_error(err);
    }
  }

  stream_t *stream =
      new_audio_stream(Av_val(_av), Int_val(_sample_fmt),
                       AVChannelLayout_val(_channel_layout), codec, &options);

  // Return unused keys
  count = av_dict_count(options);

  unused = caml_alloc_tuple(count);
  AVDictionaryEntry *entry = NULL;
  for (i = 0; i < count; i++) {
    entry = av_dict_get(options, "", entry, AV_DICT_IGNORE_SUFFIX);
    Store_field(unused, i, caml_copy_string(entry->key));
  }

  av_dict_free(&options);

  ans = caml_alloc_tuple(2);
  Store_field(ans, 0, Val_int(stream->index));
  Store_field(ans, 1, unused);

  CAMLreturn(ans);
}

static stream_t *new_video_stream(AVBufferRef *device_ctx,
                                  AVBufferRef *frame_ctx, av_t *av,
                                  const AVCodec *codec,
                                  AVDictionary **options) {
  stream_t *stream = new_stream(av, codec);

  init_stream_encoder(device_ctx, frame_ctx, av, stream, options);

  return stream;
}

CAMLprim value ocaml_av_new_video_stream(value _device_context,
                                         value _frame_context, value _av,
                                         value _codec, value _opts) {
  CAMLparam4(_device_context, _frame_context, _av, _opts);
  CAMLlocal2(ans, unused);
  const AVCodec *codec = AvCodec_val(_codec);

  AVBufferRef *device_ctx = NULL;
  AVBufferRef *frame_ctx = NULL;

  if (_device_context != Val_none)
    device_ctx = BufferRef_val(Some_val(_device_context));

  if (_frame_context != Val_none)
    frame_ctx = BufferRef_val(Some_val(_frame_context));

  AVDictionary *options = NULL;
  char *key, *val;
  int len = Wosize_val(_opts);
  int i, err, count;

  for (i = 0; i < len; i++) {
    // Dictionaries copy key/values by default!
    key = (char *)Bytes_val(Field(Field(_opts, i), 0));
    val = (char *)Bytes_val(Field(Field(_opts, i), 1));
    err = av_dict_set(&options, key, val, 0);
    if (err < 0) {
      av_dict_free(&options);
      ocaml_avutil_raise_error(err);
    }
  }

  stream_t *stream =
      new_video_stream(device_ctx, frame_ctx, Av_val(_av), codec, &options);

  // Return unused keys
  count = av_dict_count(options);

  unused = caml_alloc_tuple(count);
  AVDictionaryEntry *entry = NULL;
  for (i = 0; i < count; i++) {
    entry = av_dict_get(options, "", entry, AV_DICT_IGNORE_SUFFIX);
    Store_field(unused, i, caml_copy_string(entry->key));
  }

  av_dict_free(&options);

  ans = caml_alloc_tuple(2);
  Store_field(ans, 0, Val_int(stream->index));
  Store_field(ans, 1, unused);

  CAMLreturn(ans);
}

static stream_t *new_subtitle_stream(av_t *av, const AVCodec *codec,
                                     AVDictionary **options) {
  stream_t *stream = new_stream(av, codec);

  int ret = subtitle_header_default(stream->codec_context);
  if (ret < 0) {
    av_dict_free(options);
    ocaml_avutil_raise_error(ret);
  }

  init_stream_encoder(NULL, NULL, av, stream, options);

  return stream;
}

CAMLprim value ocaml_av_new_subtitle_stream(value _av, value _codec,
                                            value _opts) {
  CAMLparam2(_av, _opts);
  CAMLlocal2(ans, unused);
  const AVCodec *codec = AvCodec_val(_codec);

  AVDictionary *options = NULL;
  char *key, *val;
  int len = Wosize_val(_opts);
  int i, err, count;

  for (i = 0; i < len; i++) {
    // Dictionaries copy key/values by default!
    key = (char *)Bytes_val(Field(Field(_opts, i), 0));
    val = (char *)Bytes_val(Field(Field(_opts, i), 1));
    err = av_dict_set(&options, key, val, 0);
    if (err < 0) {
      av_dict_free(&options);
      ocaml_avutil_raise_error(err);
    }
  }

  stream_t *stream = new_subtitle_stream(Av_val(_av), codec, &options);

  // Return unused keys
  count = av_dict_count(options);

  unused = caml_alloc_tuple(count);
  AVDictionaryEntry *entry = NULL;
  for (i = 0; i < count; i++) {
    entry = av_dict_get(options, "", entry, AV_DICT_IGNORE_SUFFIX);
    Store_field(unused, i, caml_copy_string(entry->key));
  }

  av_dict_free(&options);

  ans = caml_alloc_tuple(2);
  Store_field(ans, 0, Val_int(stream->index));
  Store_field(ans, 1, unused);

  CAMLreturn(ans);
}

CAMLprim value ocaml_av_new_data_stream(value _av, value _codec_id,
                                        value _time_base) {
  CAMLparam2(_av, _time_base);
  CAMLlocal2(ans, unused);
  const enum AVCodecID codec_id = UnknownCodecID_val(_codec_id);
  av_t *av = Av_val(_av);

  stream_t *stream = new_stream(av, NULL);
  AVStream *s = av->format_context->streams[stream->index];

  s->time_base = rational_of_value(_time_base);
  s->codecpar->codec_type = AVMEDIA_TYPE_DATA;
  s->codecpar->codec_id = codec_id;

  CAMLreturn(Val_int(stream->index));
}

CAMLprim value ocaml_av_write_stream_packet(value _stream, value _time_base,
                                            value _packet) {
  CAMLparam3(_stream, _time_base, _packet);
  CAMLlocal1(_av);
  _av = Field(_stream, 0);
  av_t *av = Av_val(_av);
  int ret = 0, stream_index = StreamIndex_val(_stream);
  AVPacket *packet = Packet_val(_packet);
  AVStream *avstream = av->format_context->streams[stream_index];

  if (!av->streams)
    Fail("Failed to write in closed output");

  if (!av->streams[stream_index])
    caml_failwith("Internal error");

  caml_release_runtime_system();

  if (!av->header_written) {
    // write output file header
    ret = avformat_write_header(av->format_context, NULL);

    if (ret < 0) {
      caml_acquire_runtime_system();
      ocaml_avutil_raise_error(ret);
    }

    av->header_written = 1;
  }

  packet->stream_index = stream_index;
  packet->pos = -1;
  av_packet_rescale_ts(packet, rational_of_value(_time_base),
                       avstream->time_base);

  ret = av->write_frame(av->format_context, packet);

  caml_acquire_runtime_system();

  if (ret < 0)
    ocaml_avutil_raise_error(ret);

  CAMLreturn(Val_unit);
}

static void write_frame(av_t *av, int stream_index, AVCodecContext *enc_ctx,
                        value _on_keyframe, AVFrame *frame) {
  AVStream *avstream = av->format_context->streams[stream_index];
  AVFrame *hw_frame = NULL;
  int ret;

  caml_release_runtime_system();

  if (!av->header_written) {
    // write output file header
    ret = avformat_write_header(av->format_context, NULL);

    if (ret < 0) {
      caml_acquire_runtime_system();
      ocaml_avutil_raise_error(ret);
    }

    av->header_written = 1;
  }

  AVPacket *packet = av_packet_alloc();
  if (!packet) {
    caml_acquire_runtime_system();
    caml_raise_out_of_memory();
  }

  packet->data = NULL;
  packet->size = 0;

  if (enc_ctx->hw_frames_ctx && frame) {
    hw_frame = av_frame_alloc();

    if (!hw_frame) {
      av_packet_free(&packet);
      caml_acquire_runtime_system();
      caml_raise_out_of_memory();
    }

    ret = av_hwframe_get_buffer(enc_ctx->hw_frames_ctx, hw_frame, 0);

    if (ret < 0) {
      av_frame_free(&hw_frame);
      av_packet_free(&packet);
      caml_acquire_runtime_system();
      ocaml_avutil_raise_error(ret);
    }

    if (!hw_frame->hw_frames_ctx) {
      av_frame_free(&hw_frame);
      av_packet_free(&packet);
      caml_acquire_runtime_system();
      caml_raise_out_of_memory();
    }

    ret = av_hwframe_transfer_data(hw_frame, frame, 0);

    if (ret < 0) {
      av_frame_free(&hw_frame);
      av_packet_free(&packet);
      caml_acquire_runtime_system();
      ocaml_avutil_raise_error(ret);
    }

    frame = hw_frame;
  }

  // send the frame for encoding
  ret = avcodec_send_frame(enc_ctx, frame);

  if (!frame && ret == AVERROR_EOF) {
    av_packet_free(&packet);
    caml_acquire_runtime_system();
    return;
  }

  if (frame && ret == AVERROR_EOF) {
    if (hw_frame)
      av_frame_free(&hw_frame);
    av_packet_free(&packet);
    caml_acquire_runtime_system();
    ocaml_avutil_raise_error(ret);
  }

  if (ret < 0 && ret != AVERROR_EOF) {
    if (hw_frame)
      av_frame_free(&hw_frame);
    av_packet_free(&packet);
    caml_acquire_runtime_system();
    ocaml_avutil_raise_error(ret);
  }

  // read all the available output packets (in general there may be any number
  // of them
  while (ret >= 0) {
    ret = avcodec_receive_packet(enc_ctx, packet);

    if (ret < 0)
      break;

    if (packet->flags & AV_PKT_FLAG_KEY && _on_keyframe != Val_none) {
      caml_acquire_runtime_system();
      caml_callback(Field(_on_keyframe, 0), Val_unit);
      caml_release_runtime_system();
    }

    packet->stream_index = stream_index;
    packet->pos = -1;
    av_packet_rescale_ts(packet, enc_ctx->time_base, avstream->time_base);

    ret = av->write_frame(av->format_context, packet);
  }

  if (hw_frame)
    av_frame_free(&hw_frame);
  av_packet_free(&packet);

  caml_acquire_runtime_system();

  if (!frame && ret == AVERROR_EOF)
    return;

  if (ret < 0 && ret != AVERROR(EAGAIN))
    ocaml_avutil_raise_error(ret);
}

static void write_audio_frame(av_t *av, int stream_index, value _on_keyframe,
                              AVFrame *frame) {
  int err, frame_size;

  if (av->format_context->nb_streams < stream_index)
    Fail("Stream index not found!");

  stream_t *stream = av->streams[stream_index];

  if (!stream->codec_context)
    Fail("Could not find stream index");

  AVCodecContext *enc_ctx = stream->codec_context;

  write_frame(av, stream_index, enc_ctx, _on_keyframe, frame);
}

static void write_video_frame(av_t *av, int stream_index, value _on_keyframe,
                              AVFrame *frame) {
  if (av->format_context->nb_streams < stream_index)
    Fail("Stream index not found!");

  if (!av->streams)
    Fail("Failed to write in closed output");

  stream_t *stream = av->streams[stream_index];

  if (!stream->codec_context)
    Fail("Failed to write video frame with no encoder");

  AVCodecContext *enc_ctx = stream->codec_context;

  write_frame(av, stream_index, enc_ctx, _on_keyframe, frame);
}

static void write_subtitle_frame(av_t *av, int stream_index,
                                 AVSubtitle *subtitle) {
  stream_t *stream = av->streams[stream_index];

  if (av->format_context->nb_streams < stream_index)
    Fail("Stream index not found!");

  AVStream *avstream = av->format_context->streams[stream->index];
  AVCodecContext *enc_ctx = stream->codec_context;

  if (!stream->codec_context)
    Fail("Failed to write subtitle frame with no encoder");

  int err;
  int size = 512;
  AVPacket *packet = av_packet_alloc();
  if (!packet) {
    caml_raise_out_of_memory();
  }
  packet->data = NULL;
  packet->size = 0;

  err = av_new_packet(packet, size);

  if (err < 0) {
    av_packet_free(&packet);
    ocaml_avutil_raise_error(err);
  }

  caml_release_runtime_system();
  err = avcodec_encode_subtitle(stream->codec_context, packet->data,
                                packet->size, subtitle);
  caml_acquire_runtime_system();

  if (err < 0) {
    av_packet_free(&packet);
    ocaml_avutil_raise_error(err);
  }

  packet->pts = subtitle->pts;
  packet->duration = subtitle->end_display_time - subtitle->pts;
  packet->dts = subtitle->pts;
  av_packet_rescale_ts(packet, enc_ctx->time_base, avstream->time_base);

  packet->stream_index = stream_index;
  packet->pos = -1;

  caml_release_runtime_system();
  err = av->write_frame(av->format_context, packet);
  caml_acquire_runtime_system();

  av_packet_free(&packet);

  if (err < 0)
    ocaml_avutil_raise_error(err);
}

CAMLprim value ocaml_av_write_stream_frame(value _on_keyframe, value _stream,
                                           value _frame) {
  CAMLparam3(_on_keyframe, _stream, _frame);
  CAMLlocal1(_av);
  _av = Field(_stream, 0);
  av_t *av = Av_val(_av);
  int index = StreamIndex_val(_stream);

  if (!av->streams)
    Fail("Invalid input: no streams provided");

  enum AVMediaType type = av->streams[index]->codec_context->codec_type;

  if (type == AVMEDIA_TYPE_AUDIO) {
    write_audio_frame(av, index, _on_keyframe, Frame_val(_frame));
  } else if (type == AVMEDIA_TYPE_VIDEO) {
    write_video_frame(av, index, _on_keyframe, Frame_val(_frame));
  } else if (type == AVMEDIA_TYPE_SUBTITLE) {
    write_subtitle_frame(av, index, Subtitle_val(_frame));
  }

  CAMLreturn(Val_unit);
}

CAMLprim value ocaml_av_flush(value _av) {
  CAMLparam1(_av);
  av_t *av = Av_val(_av);
  int ret;

  if (!av->header_written)
    CAMLreturn(Val_unit);

  caml_release_runtime_system();
  ret = av->write_frame(av->format_context, NULL);
  if (ret >= 0 && av->format_context->pb)
    avio_flush(av->format_context->pb);
  caml_acquire_runtime_system();

  if (ret < 0)
    ocaml_avutil_raise_error(ret);

  CAMLreturn(Val_unit);
}

CAMLprim value ocaml_av_close(value _av) {
  CAMLparam1(_av);
  av_t *av = Av_val(_av);

  if (!av->is_input && av->streams) {
    // flush encoders of the output file
    unsigned int i;
    for (i = 0; i < av->format_context->nb_streams; i++) {

      AVCodecContext *enc_ctx = av->streams[i]->codec_context;

      if (!enc_ctx)
        continue;

      if (enc_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
        write_audio_frame(av, i, Val_none, NULL);
      } else if (enc_ctx->codec_type == AVMEDIA_TYPE_VIDEO) {
        write_video_frame(av, i, Val_none, NULL);
      }
    }

    // write the trailer
    if (av->header_written) {
      caml_release_runtime_system();
      av_write_trailer(av->format_context);
      caml_acquire_runtime_system();
    }
  }

  close_av(av);

  CAMLreturn(Val_unit);
}

CAMLprim value ocaml_av_cleanup_av(value _av) {
  CAMLparam1(_av);
  av_t *av = Av_base_val(_av);

  close_av(av);

  CAMLreturn(Val_unit);
}

// This is from libavformat/avc.h
uint8_t *ocaml_av_ff_nal_unit_extract_rbsp(const uint8_t *src, uint32_t src_len,
                                           uint32_t *dst_len, int header_len) {
  uint8_t *dst;
  uint32_t i, len;

  dst = av_mallocz(src_len + AV_INPUT_BUFFER_PADDING_SIZE);
  if (!dst)
    return NULL;

  /* NAL unit header */
  i = len = 0;
  while (i < header_len && i < src_len)
    dst[len++] = src[i++];

  while (i + 2 < src_len)
    if (!src[i] && !src[i + 1] && src[i + 2] == 3) {
      dst[len++] = src[i++];
      dst[len++] = src[i++];
      i++; // remove emulation_prevention_three_byte
    } else
      dst[len++] = src[i++];

  while (i < src_len)
    dst[len++] = src[i++];

  memset(dst + len, 0, AV_INPUT_BUFFER_PADDING_SIZE);

  *dst_len = len;
  return dst;
}

// This from libavformat/hlsenc.c
CAMLprim value ocaml_av_codec_attr(value _stream) {
  CAMLparam1(_stream);
  CAMLlocal3(ans, _attr, _av);

  char attr[32];
  _av = Field(_stream, 0);
  av_t *av = Av_val(_av);
  int index = StreamIndex_val(_stream);

  if (!av->format_context || !av->format_context->streams)
    CAMLreturn(Val_none);

  AVStream *stream = av->format_context->streams[index];

  if (!stream)
    CAMLreturn(Val_none);

  if (stream->codecpar->codec_id == AV_CODEC_ID_H264) {
    uint8_t *data = stream->codecpar->extradata;
    if (data && (data[0] | data[1] | data[2]) == 0 && data[3] == 1 &&
        (data[4] & 0x1F) == 7) {
      snprintf(attr, sizeof(attr), "avc1.%02x%02x%02x", data[5], data[6],
               data[7]);
    } else {
      goto fail;
    }
  } else if (stream->codecpar->codec_id == AV_CODEC_ID_FLAC) {
    snprintf(attr, sizeof(attr), "fLaC");
  } else if (stream->codecpar->codec_id == AV_CODEC_ID_HEVC) {
    uint8_t *data = stream->codecpar->extradata;
    int profile = FF_PROFILE_UNKNOWN;
    int level = FF_LEVEL_UNKNOWN;

    if (stream->codecpar->profile != FF_PROFILE_UNKNOWN)
      profile = stream->codecpar->profile;
    if (stream->codecpar->level != FF_LEVEL_UNKNOWN)
      level = stream->codecpar->level;

    /* check the boundary of data which from current position is small than
     * extradata_size */
    while (data && (data - stream->codecpar->extradata + 19) <
                       stream->codecpar->extradata_size) {
      /* get HEVC SPS NAL and seek to profile_tier_level */
      if (!(data[0] | data[1] | data[2]) && data[3] == 1 &&
          ((data[4] & 0x7E) == 0x42)) {
        uint8_t *rbsp_buf;
        int remain_size = 0;
        uint32_t rbsp_size = 0;
        /* skip start code + nalu header */
        data += 6;
        /* process by reference General NAL unit syntax */
        remain_size = stream->codecpar->extradata_size -
                      (data - stream->codecpar->extradata);
        rbsp_buf =
            ocaml_av_ff_nal_unit_extract_rbsp(data, remain_size, &rbsp_size, 0);
        if (!rbsp_buf)
          goto fail;
        if (rbsp_size < 13) {
          av_freep(&rbsp_buf);
          goto fail;
        }
        /* skip sps_video_parameter_set_id   u(4),
         *      sps_max_sub_layers_minus1    u(3),
         *  and sps_temporal_id_nesting_flag u(1) */
        profile = rbsp_buf[1] & 0x1f;
        /* skip 8 + 8 + 32 + 4 + 43 + 1 bit */
        level = rbsp_buf[12];
        av_freep(&rbsp_buf);
        goto fail;
      }
      data++;
    }
    if (stream->codecpar->codec_tag == MKTAG('h', 'v', 'c', '1') &&
        profile != FF_PROFILE_UNKNOWN && level != FF_LEVEL_UNKNOWN) {
      snprintf(attr, sizeof(attr), "%s.%d.4.L%d.B01",
               av_fourcc2str(stream->codecpar->codec_tag), profile, level);
    } else
      snprintf(attr, sizeof(attr), "%s",
               av_fourcc2str(stream->codecpar->codec_tag));
  } else if (stream->codecpar->codec_id == AV_CODEC_ID_MP2) {
    snprintf(attr, sizeof(attr), "mp4a.40.33");
  } else if (stream->codecpar->codec_id == AV_CODEC_ID_MP3) {
    snprintf(attr, sizeof(attr), "mp4a.40.34");
  } else if (stream->codecpar->codec_id == AV_CODEC_ID_AAC) {
    if (stream->codecpar->profile != FF_PROFILE_UNKNOWN)
      snprintf(attr, sizeof(attr), "mp4a.40.%d", stream->codecpar->profile + 1);
    else
      goto fail;
  } else if (stream->codecpar->codec_id == AV_CODEC_ID_AC3) {
    snprintf(attr, sizeof(attr), "ac-3");
  } else if (stream->codecpar->codec_id == AV_CODEC_ID_EAC3) {
    snprintf(attr, sizeof(attr), "ec-3");
  } else {
  fail:
    CAMLreturn(Val_none);
  }

  _attr = caml_copy_string(attr);

  ans = caml_alloc_tuple(1);
  Store_field(ans, 0, _attr);

  CAMLreturn(ans);
}

CAMLprim value ocaml_av_stream_bitrate(value _stream) {
  CAMLparam1(_stream);
  CAMLlocal2(ans, _av);

  _av = Field(_stream, 0);
  av_t *av = Av_val(_av);
  int index = StreamIndex_val(_stream);
  AVCPBProperties *props = NULL;

  if (!av->format_context || !av->format_context->streams)
    CAMLreturn(Val_none);

  AVStream *stream = av->format_context->streams[index];

  if (!stream)
    CAMLreturn(Val_none);

  if (stream->codecpar->bit_rate) {
    ans = caml_alloc_tuple(1);
    Store_field(ans, 0, Val_int(stream->codecpar->bit_rate));
    CAMLreturn(ans);
  }

#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(60, 15, 100)
  const AVPacketSideData *side_data = av_packet_side_data_get(
      stream->codecpar->coded_side_data, stream->codecpar->nb_coded_side_data,
      AV_PKT_DATA_CPB_PROPERTIES);

  if (!side_data)
    CAMLreturn(Val_none);

  props = (AVCPBProperties *)side_data->data;
#else
  props = (AVCPBProperties *)av_stream_get_side_data(
      stream, AV_PKT_DATA_CPB_PROPERTIES, NULL);
#endif

  if (!props)
    CAMLreturn(Val_none);

  ans = caml_alloc_tuple(1);
  Store_field(ans, 0, Val_int(props->max_bitrate));
  CAMLreturn(ans);
}
