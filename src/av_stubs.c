#include <string.h>

#include <caml/mlvalues.h>
#include <caml/memory.h>
#include <caml/alloc.h>
#include <caml/fail.h>
#include <caml/callback.h>
#include <caml/custom.h>
#include <caml/bigarray.h>
#include <caml/threads.h>

#include <libavutil/opt.h>
#include <libavutil/timestamp.h>
#include <libavutil/avstring.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/audio_fifo.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>

#include "avutil_stubs.h"
#include "avcodec_stubs.h"
#include "av_stubs.h"

#define Val_none Val_int(0)
#define Some_val(v) Field(v,0)

/**** Init ****/

value ocaml_av_init(value unit) {
  caml_release_runtime_system();
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58, 9, 100)
  av_register_all();
#endif
  avformat_network_init();
  caml_acquire_runtime_system();
  return Val_unit;
}

/**** Context ****/

typedef struct {
  int index;
  AVCodecContext *codec_context;

  // input
  int got_frame;

  // output
  struct SwsContext *sws_ctx;
  struct SwrContext *swr_ctx;
  AVFrame *sw_frame;
  AVAudioFifo *audio_fifo;
  AVFrame *enc_frame;
  int64_t pts;
} stream_t;

typedef struct av_t {
  AVFormatContext *format_context;
  stream_t ** streams;
  value control_message_callback;
  int is_input;

  // input
  int end_of_file;
  int selected_streams;
  int frames_pending;
  stream_t * best_audio_stream;
  stream_t * best_video_stream;
  stream_t * best_subtitle_stream;

  // output
  int header_written;
  int release_out;
  int custom_io;
} av_t;

#define Av_val(v) (*(av_t**)Data_custom_val(v))


/***** Stream handler *****/

#define StreamAv_val(v) Av_val(Field(v, 0))
#define StreamIndex_val(v) Int_val(Field(v, 1))

/**** Media Type ****/

static const enum AVMediaType MEDIA_TYPES[] = {
  AVMEDIA_TYPE_AUDIO,
  AVMEDIA_TYPE_VIDEO,
  AVMEDIA_TYPE_SUBTITLE
};
#define MEDIA_TYPES_LEN (sizeof(MEDIA_TYPES) / sizeof(enum AVMediaType))

enum AVMediaType MediaType_val(value v)
{
  return MEDIA_TYPES[Int_val(v)];
}


static void free_stream(stream_t * stream)
{
  if( ! stream) return;
  
  if(stream->codec_context) avcodec_free_context(&stream->codec_context);

  if(stream->swr_ctx) {
    swr_free(&stream->swr_ctx);
  }

  if(stream->sws_ctx) {
    sws_freeContext(stream->sws_ctx);
  }

  if(stream->sw_frame) {
    av_frame_free(&stream->sw_frame);
  }

  if(stream->audio_fifo) {
    av_audio_fifo_free(stream->audio_fifo);
  }

  if(stream->enc_frame) {
    av_frame_free(&stream->enc_frame);
  }

  free(stream);
}

static void close_av(av_t * av)
{
  if( ! av) return;

  if(av->format_context) {

    if(av->streams) {
      unsigned int i;
      for(i = 0; i < av->format_context->nb_streams; i++) {
        if(av->streams[i]) free_stream(av->streams[i]);
      }
      free(av->streams);
      av->streams = NULL;
    }

    if(av->format_context->iformat) {
      avformat_close_input(&av->format_context);
    }
    else if(av->format_context->oformat) {
      // Close the output file if needed.
      if(!av->custom_io && !(av->format_context->oformat->flags & AVFMT_NOFILE))
        avio_closep(&av->format_context->pb);

      avformat_free_context(av->format_context);
      av->format_context = NULL;
    }

    av->best_audio_stream = NULL;
    av->best_video_stream = NULL;
    av->best_subtitle_stream = NULL;
  }
}

static void free_av(av_t * av)
{
  if (!av) return;

  close_av(av);

  if(av->control_message_callback) {
    caml_acquire_runtime_system();
    caml_remove_generational_global_root(&av->control_message_callback);
    caml_release_runtime_system();
  }

  free(av);
}

CAMLprim value ocaml_av_finalize_av(value v)
{
  CAMLparam1(v);
  caml_register_generational_global_root(&v);
  caml_release_runtime_system();
  free_av(Av_val(v));
  caml_acquire_runtime_system();
  caml_remove_generational_global_root(&v);
  CAMLreturn(Val_unit);
}

static struct custom_operations av_ops = {
  "ocaml_av_context",
  custom_finalize_default,
  custom_compare_default,
  custom_hash_default,
  custom_serialize_default,
  custom_deserialize_default
};

AVFormatContext * ocaml_av_get_format_context(value *p_av) {
  return Av_val(*p_av)->format_context;
}

CAMLprim value ocaml_av_get_streams(value _av, value _media_type)
{
  CAMLparam2(_av, _media_type);
  CAMLlocal2(list, cons);
  av_t * av = Av_val(_av);
  enum AVMediaType type = MediaType_val(_media_type);
  unsigned int i;

  List_init(list);
  
  for(i = 0; i < av->format_context->nb_streams; i++) {
    if(av->format_context->streams[i]->codecpar->codec_type == type)
      List_add(list, cons, Val_int(i));
  }

  CAMLreturn(list);
}

CAMLprim value ocaml_av_get_stream_codec_parameters(value _stream) {
  CAMLparam1(_stream);
  CAMLlocal1(ans);
  av_t * av = StreamAv_val(_stream);
  int index = StreamIndex_val(_stream);

  value_of_codec_parameters_copy(av->format_context->streams[index]->codecpar, &ans);

  CAMLreturn(ans);
}

CAMLprim value ocaml_av_get_stream_time_base(value _stream) {
  CAMLparam1(_stream);
  CAMLlocal1(ans);
  av_t * av = StreamAv_val(_stream);
  int index = StreamIndex_val(_stream);

  value_of_rational(&av->format_context->streams[index]->time_base, &ans);

  CAMLreturn(ans);
}

CAMLprim value ocaml_av_set_stream_time_base(value _stream, value _time_base) {
  CAMLparam2(_stream, _time_base);
  av_t * av = StreamAv_val(_stream);
  int index = StreamIndex_val(_stream);

  av->format_context->streams[index]->time_base = rational_of_value(_time_base);

  CAMLreturn(Val_unit);
}

value * ocaml_av_get_control_message_callback(struct AVFormatContext *ctx) {
  return &((av_t *)ctx->opaque)->control_message_callback;
}

void ocaml_av_set_control_message_callback(value *p_av, av_format_control_message c_callback, value *p_ocaml_callback) {
  av_t * av = Av_val(*p_av);

  if( ! av->control_message_callback) {
    av->control_message_callback = *p_ocaml_callback;
    caml_register_generational_global_root(&av->control_message_callback);
  }
  else {
    caml_modify_generational_global_root(&av->control_message_callback, *p_ocaml_callback);
  }

  av->format_context->opaque = (void*)av;
  av->format_context->control_message_cb = c_callback;
}


/***** Input *****/

/***** AVIO *****/

typedef struct avio_t {
  AVFormatContext *format_context;
  unsigned char *buffer;
  int buffer_size;
  AVIOContext *avio_context;
  value read_cb;
  value write_cb;
  value seek_cb;
} avio_t;

#define Avio_val(v) (*(avio_t**)Data_custom_val(v))

static struct custom_operations avio_ops = {
  "ocaml_avio_ops",
  custom_finalize_default,
  custom_compare_default,
  custom_hash_default,
  custom_serialize_default,
  custom_deserialize_default
};

static int ocaml_avio_read_callback(void *private, uint8_t *buf, int buf_size) {
  value buffer, res;
  avio_t *avio = (avio_t *)private;

  caml_acquire_runtime_system();

  buffer = caml_alloc_string(buf_size);

  caml_register_generational_global_root(&buffer);

  res = caml_callback3_exn(avio->read_cb,buffer,Val_int(0),Val_int(buf_size));
  if(Is_exception_result(res)) {
    res = Extract_exception(res);
    caml_remove_generational_global_root(&buffer);
    caml_raise(res);
  }

  memcpy(buf,String_val(buffer),Int_val(res));

  caml_remove_generational_global_root(&buffer);

  caml_release_runtime_system();

  return Int_val(res);
}

static int ocaml_avio_write_callback(void *private, uint8_t *buf, int buf_size) {
  value buffer, res;
  avio_t *avio = (avio_t *)private;

  caml_acquire_runtime_system();

  buffer = caml_alloc_string(buf_size);

  memcpy(String_val(buffer), buf, buf_size);

  caml_register_generational_global_root(&buffer);

  res = caml_callback3_exn(avio->write_cb,buffer,Val_int(0),Val_int(buf_size));
  if(Is_exception_result(res)) {
    res = Extract_exception(res);
    caml_remove_generational_global_root(&buffer);
    caml_raise(res);
  }

  caml_remove_generational_global_root(&buffer);

  caml_release_runtime_system();

  return Int_val(res);
}

static int64_t ocaml_avio_seek_callback(void *private, int64_t offset, int whence) {
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

  res = caml_callback2(avio->seek_cb,Val_int(offset), Val_int(_whence));

  n = Int_val(res);

  caml_release_runtime_system();

  return n;
}

CAMLprim value ocaml_av_create_io(value bufsize, value _read_cb, value _write_cb, value _seek_cb) {
  CAMLparam3(_read_cb, _write_cb, _seek_cb);
  CAMLlocal1(ret);

  int(*read_cb)(void *opaque, uint8_t *buf, int buf_size) = NULL;
  int(*write_cb)(void *opaque, uint8_t *buf, int buf_size) = NULL;
  int64_t(*seek_cb)(void *opaque, int64_t offset, int whence) = NULL;
  int write_flag = 0;

  avio_t *avio = (avio_t *)calloc(1, sizeof(avio_t));
  if (!avio) caml_raise_out_of_memory();

  avio->read_cb = (value)NULL;
  avio->write_cb = (value)NULL;
  avio->seek_cb = (value)NULL;

  caml_release_runtime_system();
  avio->format_context = avformat_alloc_context();

  if (!avio->format_context) {
    free(avio);
    caml_acquire_runtime_system();
    caml_raise_out_of_memory();
  }

  avio->buffer_size = Int_val(bufsize);
  avio->buffer = av_malloc(avio->buffer_size);

  if (!avio->buffer) {
    av_freep(avio->format_context);
    caml_acquire_runtime_system();
    free(avio);
    caml_raise_out_of_memory();
  }

  caml_acquire_runtime_system();

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

  caml_release_runtime_system();
  avio->avio_context = avio_alloc_context(
    avio->buffer, avio->buffer_size, write_flag, (void *)avio,
    read_cb, write_cb, seek_cb);
  caml_acquire_runtime_system();

  if (!avio->avio_context) {
    caml_release_runtime_system();
    av_freep(avio->buffer);
    av_freep(avio->format_context);
    caml_acquire_runtime_system();

    free(avio);
    caml_raise_out_of_memory();
  }

  avio->format_context->pb = avio->avio_context;

  ret = caml_alloc_custom(&avio_ops, sizeof(avio_t*), 0, 1);

  Avio_val(ret) = avio;

  CAMLreturn(ret);
}

CAMLprim value caml_av_input_io_finalise(value _avio) {
  CAMLparam1(_avio);
  // format_context and the buffer are freed as part of av_close.
  avio_t *avio = Avio_val(_avio);
  
  caml_register_generational_global_root(&_avio);

  caml_release_runtime_system();
  av_freep(avio->avio_context);
  caml_acquire_runtime_system(); 

  if (avio->read_cb)
    caml_remove_generational_global_root(&avio->read_cb);

  if (avio->write_cb)
    caml_remove_generational_global_root(&avio->write_cb);

  if (avio->seek_cb)
    caml_remove_generational_global_root(&avio->seek_cb);

  free(avio);

  caml_remove_generational_global_root(&_avio);

  CAMLreturn(Val_unit);
}

/***** AVInputFormat *****/

static struct custom_operations inputFormat_ops = {
  "ocaml_av_inputformat",
  custom_finalize_default,
  custom_compare_default,
  custom_hash_default,
  custom_serialize_default,
  custom_deserialize_default
};

void value_of_inputFormat(AVInputFormat *inputFormat, value * p_value)
{
  if( ! inputFormat) Fail("Empty input format");

  *p_value = caml_alloc_custom(&inputFormat_ops, sizeof(AVInputFormat*), 0, 1);
  InputFormat_val((*p_value)) = inputFormat;
}

CAMLprim value ocaml_av_input_format_get_name(value _format)
{
  CAMLparam1(_format);
  const char * n = InputFormat_val(_format)->name;
  CAMLreturn(caml_copy_string(n ? n : ""));
}

CAMLprim value ocaml_av_input_format_get_long_name(value _format)
{
  CAMLparam1(_format);
  const char * n = InputFormat_val(_format)->long_name;
  CAMLreturn(caml_copy_string(n ? n : ""));
}


static av_t * open_input(char *url, AVInputFormat *format, AVFormatContext *format_context)
{
  int err;

  av_t *av = (av_t*)calloc(1, sizeof(av_t));
  if ( ! av) {
    if (url) free(url);
    caml_raise_out_of_memory();
  }

  av->is_input = 1;
  av->release_out = 1;
  av->frames_pending = 0;
  av->format_context = format_context;
  av->streams = NULL;
  
  caml_release_runtime_system();
  err = avformat_open_input(&av->format_context, url, format, NULL);
  caml_acquire_runtime_system();
  
  if (err < 0) {
    free(av);
    if (url) free(url);
    ocaml_avutil_raise_error(err);
  }

  // retrieve stream information
  caml_release_runtime_system();
  err = avformat_find_stream_info(av->format_context, NULL);
  caml_acquire_runtime_system();

  if (err < 0) {
    free(av);
    if (url) free(url);
    ocaml_avutil_raise_error(err);
  }

  free(url);

  return av;
}

CAMLprim value ocaml_av_open_input(value _url)
{
  CAMLparam1(_url);
  CAMLlocal1(ans);
  char * url = strndup(String_val(_url), caml_string_length(_url));

  // open input url
  av_t *av = open_input(url, NULL, NULL);

  // allocate format context
  ans = caml_alloc_custom(&av_ops, sizeof(av_t*), 0, 1);
  Av_val(ans) = av;

  Finalize("ocaml_av_finalize_av",ans);

  CAMLreturn(ans);
}

CAMLprim value ocaml_av_open_input_format(value _format)
{
  CAMLparam1(_format);
  CAMLlocal1(ans);
  AVInputFormat *format = InputFormat_val(_format);

  // open input format
  av_t *av = open_input(NULL, format, NULL);

  // allocate format context
  ans = caml_alloc_custom(&av_ops, sizeof(av_t*), 0, 1);
  Av_val(ans) = av;

  Finalize("ocaml_av_finalize_av",ans);

  CAMLreturn(ans);
}

CAMLprim value ocaml_av_open_input_stream(value _avio)
{
  CAMLparam1(_avio);
  CAMLlocal1(ans);
  avio_t *avio = Avio_val(_avio);

  // open input format
  av_t *av = open_input(NULL, NULL, avio->format_context);

  // allocate format context
  ans = caml_alloc_custom(&av_ops, sizeof(av_t*), 0, 1);
  Av_val(ans) = av;

  CAMLreturn(ans);
}

CAMLprim value ocaml_av_get_metadata(value _av, value _stream_index)
{
  CAMLparam1(_av);
  CAMLlocal3(pair, cons, list);
  av_t * av = Av_val(_av);
  int index = Int_val(_stream_index);
  AVDictionary * metadata = av->format_context->metadata;
  AVDictionaryEntry *tag = NULL;

  if(index >= 0) {
    metadata = av->format_context->streams[index]->metadata;
  }

  List_init(list);

  caml_release_runtime_system();

  while((tag = av_dict_get(metadata, "", tag, AV_DICT_IGNORE_SUFFIX))) {

    caml_acquire_runtime_system();

    pair = caml_alloc_tuple(2);
    Store_field(pair, 0, caml_copy_string(tag->key));
    Store_field(pair, 1, caml_copy_string(tag->value));

    List_add(list, cons, pair);

    caml_release_runtime_system();
  }

  caml_acquire_runtime_system();

  CAMLreturn(list);
}

CAMLprim value ocaml_av_get_duration(value _av, value _stream_index, value _time_format)
{
  CAMLparam2(_av, _time_format);
  CAMLlocal1(ans);
  av_t * av = Av_val(_av);
  int index = Int_val(_stream_index);
  
  if( ! av->format_context) Fail("Failed to get closed input duration");

  int64_t duration = av->format_context->duration;
  int64_t num = 1;
  int64_t den = AV_TIME_BASE;
  
  if(index >= 0) {
    duration = av->format_context->streams[index]->duration;
    num = (int64_t)av->format_context->streams[index]->time_base.num;
    den = (int64_t)av->format_context->streams[index]->time_base.den;
  }

  int64_t second_fractions = second_fractions_of_time_format(_time_format);

  ans = caml_copy_int64((duration * second_fractions * num) / den);

  CAMLreturn(ans);
}

CAMLprim value ocaml_av_reuse_output(value _av, value _reuse_output)
{
  CAMLparam2(_av, _reuse_output);
  Av_val(_av)->release_out = ! Bool_val(_reuse_output);
  CAMLreturn(Val_unit);
}

static inline stream_t** allocate_input_context(av_t *av)
{
  if( ! av->format_context) Fail("Failed to read closed input");
  
  // Allocate streams context array
  av->streams = (stream_t**)calloc(av->format_context->nb_streams, sizeof(stream_t*));
  if( ! av->streams) caml_raise_out_of_memory();

  return av->streams;
}

static inline stream_t *allocate_stream_context(av_t *av, int index, AVCodec *codec)
{
  enum AVMediaType type = codec->type;

  if(type != AVMEDIA_TYPE_AUDIO && type != AVMEDIA_TYPE_VIDEO && type != AVMEDIA_TYPE_SUBTITLE) Fail("Failed to allocate stream %d of media type %s", index, av_get_media_type_string(type));

  stream_t * stream = (stream_t*)calloc(1, sizeof(stream_t));
  if( ! stream) caml_raise_out_of_memory();

  stream->index = index;
  av->streams[index] = stream;

  caml_release_runtime_system();
  stream->codec_context = avcodec_alloc_context3(codec);
  caml_acquire_runtime_system();

  if( ! stream->codec_context) caml_raise_out_of_memory();

  return stream;
}

static inline stream_t *open_stream_index(av_t *av, int index)
{
  int err;

  if(!av->format_context) Fail("Failed to open stream %d of closed input", index);

  if(index < 0 || index >= av->format_context->nb_streams) Fail("Failed to open stream %d : index out of bounds", index);
  
  if(!av->streams && !allocate_input_context(av)) caml_raise_out_of_memory();

  // find decoder for the stream
  AVCodecParameters *dec_param = av->format_context->streams[index]->codecpar;

  caml_release_runtime_system();
  AVCodec *dec = avcodec_find_decoder(dec_param->codec_id);
  caml_acquire_runtime_system();

  if( ! dec) ocaml_avutil_raise_error(AVERROR_DECODER_NOT_FOUND);
  
  stream_t *stream = allocate_stream_context(av, index, dec);
  if ( ! stream) caml_raise_out_of_memory();

  // initialize the stream parameters with demuxer information
  caml_release_runtime_system();
  err = avcodec_parameters_to_context(stream->codec_context, dec_param);

  if (err < 0) {
    caml_acquire_runtime_system();
    ocaml_avutil_raise_error(err);
  }

  // Open the decoder
  err = avcodec_open2(stream->codec_context, dec, NULL);
  caml_acquire_runtime_system();

  if(err < 0) ocaml_avutil_raise_error(err);

  return stream;
}

#define Check_stream(av, index) {                     \
    if( ! (av)->streams || ! (av)->streams[(index)])  \
      open_stream_index((av), (index));               \
  }

CAMLprim value ocaml_av_find_best_stream(value _av, value _media_type)
{
  CAMLparam2(_av, _media_type);
  av_t * av = Av_val(_av);
  enum AVMediaType type = MediaType_val(_media_type);

  caml_release_runtime_system();
  int index = av_find_best_stream(av->format_context, type, -1, -1, NULL, 0);
  caml_acquire_runtime_system();

  if(index < 0) ocaml_avutil_raise_error(AVERROR_STREAM_NOT_FOUND);

  CAMLreturn(Val_int(index));
}


CAMLprim value ocaml_av_select_stream(value _stream)
{
  CAMLparam1(_stream);
  av_t * av = StreamAv_val(_stream);
  int index = StreamIndex_val(_stream);

  Check_stream(av, index);

  av->selected_streams = 1;

  CAMLreturn(Val_unit);
}


static inline int decode_packet(av_t * av, stream_t * stream, AVPacket * packet, AVFrame * frame)
{
  AVCodecContext * dec = stream->codec_context;
  int ret = 0;

  caml_release_runtime_system();

  if(dec->codec_type == AVMEDIA_TYPE_AUDIO ||
     dec->codec_type == AVMEDIA_TYPE_VIDEO) {
  
    // Assumption: each time this function is called with `frames_pending == 0`, 
    // a fresh packet is also provided and no packet otherwise.
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
  }
  else if(dec->codec_type == AVMEDIA_TYPE_SUBTITLE) {
    ret = avcodec_decode_subtitle2(dec, (AVSubtitle *)frame, &stream->got_frame, packet);
    if (ret >=0) av_packet_unref(packet);
  }

  av_packet_unref(packet);

  caml_acquire_runtime_system();

  stream->got_frame = 1;

  return ret;
}

static inline void read_packet(av_t * av, AVPacket * packet, int stream_index, stream_t ** selected_streams)
{
  int ret;

  caml_release_runtime_system();

  for(;;) {
    // read frames from the file
    ret = av_read_frame(av->format_context, packet);

    if(ret == AVERROR_EOF) {
      packet->data = NULL;
      packet->size = 0;
      av->end_of_file = 1;
      caml_acquire_runtime_system();
      return;
    }

    if(ret < 0) {
      caml_acquire_runtime_system();
      ocaml_avutil_raise_error(ret);
    }

    if(packet->stream_index == stream_index
       || (stream_index < 0
           && ( ! selected_streams
                || selected_streams[packet->stream_index]))) break;

    av_packet_unref(packet);
  }

  caml_acquire_runtime_system();
}


CAMLprim value ocaml_av_read_stream_packet(value _stream) {
  CAMLparam1(_stream);

  av_t * av = StreamAv_val(_stream);
  int index = StreamIndex_val(_stream);

  Check_stream(av, index);

  caml_release_runtime_system();
  AVPacket *packet = av_packet_alloc();
  caml_acquire_runtime_system();

  if(!packet) caml_raise_out_of_memory();

  read_packet(av, packet, index, NULL);

  if(av->end_of_file) {
    caml_release_runtime_system();
    av_packet_free(&packet);
    caml_acquire_runtime_system();

    ocaml_avutil_raise_error(AVERROR_EOF);
  }

  CAMLreturn(value_of_ffmpeg_packet(packet));
}


CAMLprim value ocaml_av_read_stream_frame(value _stream) {
  CAMLparam1(_stream);
  CAMLlocal1(frame_value);
  av_t * av = StreamAv_val(_stream);
  int index = StreamIndex_val(_stream);
  int ret;
  AVFrame *frame;

  Check_stream(av, index);

  stream_t * stream = av->streams[index];

  AVPacket packet;
  av_init_packet(&packet);
  packet.data = NULL;
  packet.size = 0;

  // Allocate and assign a OCaml value right away to account
  // for potential exceptions raised afterward.
  if (stream->codec_context->codec_type == AVMEDIA_TYPE_SUBTITLE) {
    frame = (AVFrame *)calloc(1, sizeof(AVSubtitle));

    if (!frame) caml_raise_out_of_memory();

    frame_value = value_of_subtitle((AVSubtitle*)frame);
  } else {
    caml_release_runtime_system();
    frame = av_frame_alloc();
    caml_acquire_runtime_system();

    if (!frame) caml_raise_out_of_memory();

    frame_value = value_of_frame(frame);
  }
  
  do {
    if(!av->frames_pending)
      read_packet(av, &packet, index, NULL);

    ret = decode_packet(av, stream, &packet, frame);
  } while (ret == AVERROR(EAGAIN));

  // Leave it to the GC to cleanup the frame above.
  if (ret < 0 && ret != AVERROR(EAGAIN))
    ocaml_avutil_raise_error(ret);

  CAMLreturn(frame_value);
}


CAMLprim value ocaml_av_read_input_packet(value _av) {
  CAMLparam1(_av);
  CAMLlocal2(ans, stream_packet);
  av_t * av = Av_val(_av);
  stream_t ** selected_streams = av->selected_streams ? av->streams : NULL;

  if(! av->streams && ! allocate_input_context(av)) caml_raise_out_of_memory();

  caml_release_runtime_system();
  AVPacket *packet = av_packet_alloc();
  caml_acquire_runtime_system();

  if(!packet) caml_raise_out_of_memory();

  read_packet(av, packet, -1, selected_streams);

  if(av->end_of_file) {
    caml_release_runtime_system();
    av_packet_free(&packet);
    caml_acquire_runtime_system();

    ocaml_avutil_raise_error(AVERROR_EOF);
  }

  int index = packet->stream_index;

  Check_stream(av, index);

  stream_packet = caml_alloc_tuple(2);
  Field(stream_packet, 0) = Val_int(index);
  Field(stream_packet, 1) = value_of_ffmpeg_packet(packet);

  ans = caml_alloc_tuple(2);
  switch(av->streams[index]->codec_context->codec_type) {
    case AVMEDIA_TYPE_AUDIO :
      Field(ans, 0) = PVV_Audio;
      break;
    case AVMEDIA_TYPE_VIDEO:
      Field(ans, 0) = PVV_Video;
      break;
  default:
      Field(ans, 0) = PVV_Subtitle;
      break;
  }

  Field(ans, 1) = stream_packet;

  CAMLreturn(ans);
}


CAMLprim value ocaml_av_read_input_frame(value _av)
{
  CAMLparam1(_av);
  CAMLlocal3(ans, stream_frame, frame_value);
  av_t * av = Av_val(_av);
  AVFrame * frame;
  int ret, frame_kind;

  if(! av->streams && ! allocate_input_context(av)) caml_raise_out_of_memory();

  AVPacket packet;
  av_init_packet(&packet);
  packet.data = NULL;
  packet.size = 0;

  stream_t ** streams = av->streams;
  stream_t ** selected_streams = av->selected_streams ? av->streams : NULL;
  unsigned int nb_streams = av->format_context->nb_streams;
  stream_t * stream = NULL;

  do {
    if(!av->end_of_file) {
  
      if(!av->frames_pending) {
        read_packet(av, &packet, -1, selected_streams);

        if(av->end_of_file) continue;
      }
  
      if((stream = streams[packet.stream_index]) == NULL)
        stream = open_stream_index(av, packet.stream_index);
    }
    else {
      // If the end of file is reached, iteration on the streams to find one to flush
      unsigned int i = 0;
      for(; i < nb_streams; i++) {
        if((stream = streams[i]) && stream->got_frame) break;
      }

      if(i == nb_streams) ocaml_avutil_raise_error(AVERROR_EOF);
    }

    // Assign OCaml values right away to account for potential exceptions
    // raised below.
    if (stream->codec_context->codec_type == AVMEDIA_TYPE_SUBTITLE) {
      frame = (AVFrame *)calloc(1, sizeof(AVSubtitle));

      if( ! frame) caml_raise_out_of_memory();

      frame_kind = PVV_Subtitle;
      frame_value = value_of_subtitle((AVSubtitle*)frame);
    } else {
      caml_release_runtime_system();
      frame = av_frame_alloc();
      caml_acquire_runtime_system();

      if( ! frame) caml_raise_out_of_memory();

      if (stream->codec_context->codec_type == AVMEDIA_TYPE_AUDIO)
        frame_kind = PVV_Audio;
      else
        frame_kind = PVV_Video;

      frame_value = value_of_frame(frame);
    }

    ret = decode_packet(av, stream, &packet, frame);
  } while (ret == AVERROR(EAGAIN));

  if (ret < 0 && ret != AVERROR(EAGAIN))
    ocaml_avutil_raise_error(ret);

  stream_frame = caml_alloc_tuple(2);
  Field(stream_frame, 0) = Val_int(stream->index);
  Field(stream_frame, 1) = frame_value;

  ans = caml_alloc_tuple(2);
  Field(ans, 0) = frame_kind;
  Field(ans, 1) = stream_frame;

  CAMLreturn(ans);
}


static const int seek_flags[] = {AVSEEK_FLAG_BACKWARD, AVSEEK_FLAG_BYTE, AVSEEK_FLAG_ANY, AVSEEK_FLAG_FRAME};

static int seek_flags_val(value v)
{
  return seek_flags[Int_val(v)];
}

CAMLprim value ocaml_av_seek_frame(value _stream, value _time_format, value _timestamp, value _flags)
{
  CAMLparam4(_stream, _time_format, _timestamp, _flags);
  av_t * av = StreamAv_val(_stream);
  int index = StreamIndex_val(_stream);
  int64_t timestamp = Int64_val(_timestamp);

  if( ! av->format_context) Fail("Failed to seek closed input");
  
  int64_t num = AV_TIME_BASE;
  int64_t den = 1;
  
  if(index >= 0) {
    num = (int64_t)av->format_context->streams[index]->time_base.den;
    den = (int64_t)av->format_context->streams[index]->time_base.num;
  }

  int64_t second_fractions = second_fractions_of_time_format(_time_format);

  timestamp = (timestamp * num) / (den * second_fractions);

  int flags = 0;
  int i;
  for(i = 0; i < Wosize_val(_flags); i++)
    flags |= seek_flags_val(Field(_flags, i));

  caml_release_runtime_system();

  int ret = av_seek_frame(av->format_context, index, timestamp, flags);

  caml_acquire_runtime_system();

  if(ret < 0) ocaml_avutil_raise_error(ret);

  CAMLreturn(Val_unit);
}


/***** Output *****/

/***** AVOutputFormat *****/

static struct custom_operations outputFormat_ops = {
  "ocaml_av_outputformat",
  custom_finalize_default,
  custom_compare_default,
  custom_hash_default,
  custom_serialize_default,
  custom_deserialize_default
};

value value_of_outputFormat(AVOutputFormat *outputFormat)
{
  value v;
  if( ! outputFormat) Fail("Empty output format");

  v = caml_alloc_custom(&outputFormat_ops, sizeof(AVOutputFormat*), 0, 1);
  OutputFormat_val(v) = outputFormat;

  return v;
}

CAMLprim value ocaml_av_output_format_guess(value _short_name, value _filename, value _mime)
{
  CAMLparam3(_short_name, _filename, _mime);
  CAMLlocal1(ans);
  char *short_name = NULL;
  char *filename = NULL;
  char *mime = NULL;
  AVOutputFormat *guessed;

  if (caml_string_length(_short_name) > 0) {
    short_name = calloc(caml_string_length(_short_name), sizeof(char));
    if (!short_name) caml_raise_out_of_memory();

    memcpy(short_name, String_val(_short_name), caml_string_length(_short_name));
  };

  if (caml_string_length(_filename) > 0) {
    filename = calloc(caml_string_length(_filename), sizeof(char));
    if (!filename) {
      if (short_name) free(short_name);
      caml_raise_out_of_memory();
    }
  
    memcpy(filename, String_val(_filename), caml_string_length(_filename));
  }

  if (caml_string_length(_mime) > 0) {
    mime = calloc(caml_string_length(_mime), sizeof(char));
    if (!mime) {
      if (short_name) free(short_name);
      if (filename) free(filename);
      caml_raise_out_of_memory();
    }

    memcpy(mime, String_val(_mime), caml_string_length(_mime));
  }

  caml_release_runtime_system();
  guessed = av_guess_format(short_name, filename, mime);
  caml_acquire_runtime_system();

  if (short_name) free(short_name);
  if (filename) free(filename);
  if (mime) free(mime);

  if (!guessed) CAMLreturn(Val_none);

  ans = caml_alloc_small(1, 0);
  Field(ans, 0) = value_of_outputFormat(guessed);

  CAMLreturn(ans); 
}

CAMLprim value ocaml_av_output_format_get_name(value _format)
{
  CAMLparam1(_format);
  const char * n = OutputFormat_val(_format)->name;
  CAMLreturn(caml_copy_string(n ? n : ""));
}

CAMLprim value ocaml_av_output_format_get_long_name(value _format)
{
  CAMLparam1(_format);
  const char * n = OutputFormat_val(_format)->long_name;
  CAMLreturn(caml_copy_string(n ? n : ""));
}

CAMLprim value ocaml_av_output_format_get_audio_codec_id(value _output_format)
{
  CAMLparam1(_output_format);
  CAMLreturn(Val_AudioCodecID(OutputFormat_val(_output_format)->audio_codec));
}

CAMLprim value ocaml_av_output_format_get_video_codec_id(value _output_format)
{
  CAMLparam1(_output_format);
  CAMLreturn(Val_VideoCodecID(OutputFormat_val(_output_format)->video_codec));
}

CAMLprim value ocaml_av_output_format_get_subtitle_codec_id(value _output_format)
{
  CAMLparam1(_output_format);
  CAMLreturn(Val_SubtitleCodecID(OutputFormat_val(_output_format)->subtitle_codec));
}


static inline av_t * open_output(AVOutputFormat *format, char *file_name, AVIOContext *avio_context)
{
  av_t *av = (av_t*)calloc(1, sizeof(av_t));
  if ( ! av) {
    if (file_name) free(file_name);
    caml_raise_out_of_memory();
  }

  caml_release_runtime_system();

  avformat_alloc_output_context2(&av->format_context, format, NULL, file_name);

  if ( ! av->format_context) {
    free_av(av);
    if (file_name) free(file_name);

    caml_acquire_runtime_system();
    caml_raise_out_of_memory();
  }

  // open the output file, if needed
  if (avio_context) {
    if (av->format_context->oformat->flags & AVFMT_NOFILE) {
      free_av(av);
      if (file_name) free(file_name);

      caml_acquire_runtime_system();
      Fail("Cannot set custom I/O on this format!");
    }

    av->format_context->pb = avio_context;
    av->custom_io = 1;
  } else {
    if(!(av->format_context->oformat->flags & AVFMT_NOFILE)) {
      int err = avio_open(&av->format_context->pb, file_name, AVIO_FLAG_WRITE);

      if (err < 0) {
        free_av(av);
        if (file_name) free(file_name);

        caml_acquire_runtime_system();
        ocaml_avutil_raise_error(err);
      }

      av->custom_io = 0;
    }
  }

  if (file_name) free(file_name);

  caml_acquire_runtime_system();

  return av;
}

CAMLprim value ocaml_av_open_output(value _filename)
{
  CAMLparam1(_filename);
  CAMLlocal1(ans);
  char * filename = strndup(String_val(_filename), caml_string_length(_filename));

  // open output file
  av_t *av = open_output(NULL, filename, NULL);

  // allocate format context
  ans = caml_alloc_custom(&av_ops, sizeof(av_t*), 0, 1);
  Av_val(ans) = av;

  CAMLreturn(ans);
}

CAMLprim value ocaml_av_open_output_format(value _format)
{
  CAMLparam1(_format);
  CAMLlocal1(ans);
  AVOutputFormat *format = OutputFormat_val(_format);

  // open output format
  av_t *av = open_output(format, NULL, NULL);

  // allocate format context
  ans = caml_alloc_custom(&av_ops, sizeof(av_t*), 0, 1);
  Av_val(ans) = av;

  CAMLreturn(ans);
}

CAMLprim value ocaml_av_open_output_stream(value _format, value _avio)
{
  CAMLparam2(_format, _avio);
  CAMLlocal1(ans);
  AVOutputFormat *format = OutputFormat_val(_format);
  avio_t *avio = Avio_val(_avio);

  // open output format
  av_t *av = open_output(format, NULL, avio->avio_context);

  // allocate format context
  ans = caml_alloc_custom(&av_ops, sizeof(av_t*), 0, 1);
  Av_val(ans) = av;

  CAMLreturn(ans);
}

CAMLprim value ocaml_av_set_metadata(value _av, value _stream_index, value _tags) {
  CAMLparam2(_av, _tags);
  CAMLlocal1(pair);
  av_t * av = Av_val(_av);
  int index = Int_val(_stream_index);
  AVDictionary * metadata = NULL;

  if( ! av->format_context) Fail("Failed to set metadata to closed output");
  if(av->header_written) Fail("Failed to set metadata : header already written");

  int i, ret, len = Wosize_val(_tags);
  
  for(i = 0; i < len; i++) {

    pair = Field(_tags, i);

    ret = av_dict_set(&metadata,
                      String_val(Field(pair, 0)),
                      String_val(Field(pair, 1)),
                      0);
    if (ret < 0) ocaml_avutil_raise_error(ret);
  }

  if(index < 0) {
    av->format_context->metadata = metadata;
  }
  else {
    av->format_context->streams[index]->metadata = metadata;
  }

  CAMLreturn(Val_unit);
}

static inline stream_t * new_stream(av_t *av, enum AVCodecID codec_id)
{
  if(!av->format_context) Fail("Failed to add stream to closed output");
  if(av->header_written) Fail("Failed to create new stream : header already written");

  caml_release_runtime_system();
  AVCodec * encoder = avcodec_find_encoder(codec_id);
  caml_acquire_runtime_system();

  if( ! encoder) Fail("Failed to find %s encoder", avcodec_get_name(codec_id));

  // Allocate streams array
  size_t streams_size = sizeof(stream_t*) * (av->format_context->nb_streams + 1);
  stream_t**streams = (stream_t**)realloc(av->streams, streams_size);
  if( ! streams) caml_raise_out_of_memory();

  streams[av->format_context->nb_streams] = NULL;
  av->streams = streams;

  stream_t * stream = allocate_stream_context(av, av->format_context->nb_streams, encoder);
  if ( ! stream) caml_raise_out_of_memory();

  AVStream * avstream = avformat_new_stream(av->format_context, NULL);
  if( ! avstream) caml_raise_out_of_memory();

  avstream->id = av->format_context->nb_streams - 1;

  return stream;
}

static inline void init_stream_encoder(av_t *av, stream_t *stream)
{
  AVCodecContext *enc_ctx = stream->codec_context;

  // Some formats want stream headers to be separate.
  if (av->format_context->oformat->flags & AVFMT_GLOBALHEADER)
    enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

  caml_release_runtime_system();
  int ret = avcodec_open2(enc_ctx, NULL, NULL);

  if (ret < 0) {
    caml_acquire_runtime_system();
    ocaml_avutil_raise_error(ret);
  }

  AVStream *avstream = av->format_context->streams[stream->index];
  avstream->time_base = enc_ctx->time_base;
  
  ret = avcodec_parameters_from_context(avstream->codecpar, enc_ctx);

  caml_acquire_runtime_system();

  if (ret < 0) ocaml_avutil_raise_error(ret);
}

static inline stream_t *new_audio_stream(av_t *av, enum AVCodecID codec_id, uint64_t channel_layout, enum AVSampleFormat sample_fmt, int bit_rate, int sample_rate, AVRational time_base)
{
  stream_t * stream = new_stream(av, codec_id);
  
  AVCodecContext *enc_ctx = stream->codec_context;

  enc_ctx->bit_rate = bit_rate;
  enc_ctx->sample_rate = sample_rate;
  enc_ctx->channel_layout = channel_layout;
  enc_ctx->channels = av_get_channel_layout_nb_channels(channel_layout);
  enc_ctx->sample_fmt = sample_fmt;
  enc_ctx->time_base = time_base;

  init_stream_encoder(av, stream);

  if (enc_ctx->frame_size > 0
      || ! (enc_ctx->codec->capabilities & AV_CODEC_CAP_VARIABLE_FRAME_SIZE)) {
    caml_release_runtime_system();

    // Allocate the buffer frame and audio fifo if the codec doesn't support variable frame size
    stream->enc_frame = av_frame_alloc();
    if( ! stream->enc_frame) {
      caml_acquire_runtime_system();
      caml_raise_out_of_memory();
    }

    stream->enc_frame->nb_samples     = enc_ctx->frame_size;
    stream->enc_frame->channel_layout = enc_ctx->channel_layout;
    stream->enc_frame->format         = enc_ctx->sample_fmt;
    stream->enc_frame->sample_rate    = enc_ctx->sample_rate;

    int ret = av_frame_get_buffer(stream->enc_frame, 0);

    if (ret < 0) {
      caml_acquire_runtime_system();
      ocaml_avutil_raise_error(ret);
     }
  
    // Create the FIFO buffer based on the specified output sample format.
    stream->audio_fifo = av_audio_fifo_alloc(enc_ctx->sample_fmt, enc_ctx->channels, 1);

    caml_release_runtime_system();

    if(!stream->audio_fifo) caml_raise_out_of_memory();
  }

  return stream;
}

CAMLprim value ocaml_av_new_audio_stream(value _av, value _audio_codec_id, value _channel_layout, value _sample_fmt, value _bit_rate, value _sample_rate, value _time_base)
{
  CAMLparam5(_av, _audio_codec_id, _channel_layout, _sample_fmt, _time_base);

  stream_t * stream = new_audio_stream(Av_val(_av),
                                       AudioCodecID_val(_audio_codec_id),
                                       ChannelLayout_val(_channel_layout),
                                       SampleFormat_val(_sample_fmt),
                                       Int_val(_bit_rate),
                                       Int_val(_sample_rate),
                                       rational_of_value(_time_base));

  CAMLreturn(Val_int(stream->index));
}

CAMLprim value ocaml_av_new_audio_stream_byte(value *argv, int argn)
{
  return ocaml_av_new_audio_stream(argv[0], argv[1], argv[2], argv[3], argv[4], argv[5], argv[6]);
}


static inline stream_t * new_video_stream(av_t *av, enum AVCodecID codec_id, int width, int height, enum AVPixelFormat pix_fmt, int bit_rate, int frame_rate, AVRational time_base)
{
  stream_t * stream = new_stream(av, codec_id);
  if( ! stream) return NULL;
  
  AVCodecContext * enc_ctx = stream->codec_context;

  enc_ctx->bit_rate = bit_rate;
  enc_ctx->width = width;
  enc_ctx->height = height;
  enc_ctx->pix_fmt = pix_fmt;
  enc_ctx->time_base = time_base;
  enc_ctx->framerate = (AVRational){frame_rate, 1};
 
  init_stream_encoder(av, stream);
  return stream;
}

CAMLprim value ocaml_av_new_video_stream(value _av, value _video_codec_id, value _width, value _height, value _pix_fmt, value _bit_rate, value _frame_rate, value _time_base)
{
  CAMLparam4(_av, _video_codec_id, _pix_fmt, _time_base);

  stream_t * stream = new_video_stream(
                                       Av_val(_av),
                                       VideoCodecID_val(_video_codec_id),
                                       Int_val(_width),
                                       Int_val(_height),
                                       PixelFormat_val(_pix_fmt),
                                       Int_val(_bit_rate),
                                       Int_val(_frame_rate),
                                       rational_of_value(_time_base));

  CAMLreturn(Val_int(stream->index));
}

CAMLprim value ocaml_av_new_video_stream_byte(value *argv, int argn)
{
  return ocaml_av_new_video_stream(argv[0], argv[1], argv[2], argv[3], argv[4], argv[5], argv[6], argv[7]);
}


static inline stream_t * new_subtitle_stream(av_t *av, enum AVCodecID codec_id, AVRational time_base)
{
  stream_t * stream = new_stream(av, codec_id);
  if( ! stream) return NULL;

  int ret = subtitle_header_default(stream->codec_context);
  if (ret < 0) ocaml_avutil_raise_error(ret);

  stream->codec_context->time_base = time_base;

  init_stream_encoder(av, stream);
  return stream;
}

CAMLprim value ocaml_av_new_subtitle_stream(value _av, value _subtitle_codec_id, value _time_base)
{
  CAMLparam3(_av, _subtitle_codec_id, _time_base);

  stream_t * stream = new_subtitle_stream(Av_val(_av),
                                          SubtitleCodecID_val(_subtitle_codec_id),
                                          rational_of_value(_time_base));

  CAMLreturn(Val_int(stream->index));
}

CAMLprim value ocaml_av_write_stream_packet(value _stream, value _packet)
{
  CAMLparam2(_stream, _packet);
  av_t * av = StreamAv_val(_stream);
  int ret = 0, stream_index = StreamIndex_val(_stream);
  AVPacket *packet = Packet_val(_packet);

  if( ! av->streams) Fail("Failed to write in closed output");

  caml_release_runtime_system();

  if( ! av->header_written) {
    // write output file header
    ret = avformat_write_header(av->format_context, NULL);
    av->header_written = 1;
  }

  if(ret >= 0) {
    AVCodecContext * enc_ctx = av->streams[stream_index]->codec_context;
    AVStream * avstream = av->format_context->streams[stream_index];

    packet->stream_index = stream_index;
    packet->pos = -1;

    // The stream time base can be modified by avformat_write_header so the rescale is needed
    av_packet_rescale_ts(packet, enc_ctx->time_base, avstream->time_base);

    ret = av_interleaved_write_frame(av->format_context, packet);
  }

  caml_acquire_runtime_system();

  if(ret < 0) ocaml_avutil_raise_error(ret);
    
  CAMLreturn(Val_unit);
}

static inline void write_frame(av_t * av, int stream_index, AVCodecContext * enc_ctx, AVFrame * frame)
{
  AVStream * avstream = av->format_context->streams[stream_index];
  int ret;

  caml_release_runtime_system();

  if(!av->header_written) {
    // write output file header
    ret = avformat_write_header(av->format_context, NULL);

    if(ret < 0) {
      caml_acquire_runtime_system();
      ocaml_avutil_raise_error(ret); 
    }

    av->header_written = 1;
  }

  AVPacket packet;
  av_init_packet(&packet);
  packet.data = NULL;
  packet.size = 0;

  // send the frame for encoding
  ret = avcodec_send_frame(enc_ctx, frame);

  if (!frame && ret == AVERROR_EOF) {
    caml_acquire_runtime_system();
    return;
  }

  if (frame && ret == AVERROR_EOF) {
     caml_acquire_runtime_system();
     ocaml_avutil_raise_error(ret);
  }

  if (ret < 0 && ret != AVERROR_EOF) {
    caml_acquire_runtime_system();
    ocaml_avutil_raise_error(ret);
  }

  // read all the available output packets (in general there may be any number of them
  while (ret >= 0) {
    ret = avcodec_receive_packet(enc_ctx, &packet);

    if (ret < 0) break;

    packet.stream_index = stream_index;
    packet.pos = -1;
    av_packet_rescale_ts(&packet, enc_ctx->time_base, avstream->time_base);

    ret = av_interleaved_write_frame(av->format_context, &packet);

    av_packet_unref(&packet);
  }

  caml_acquire_runtime_system();

  if (!frame && ret == AVERROR_EOF) return;

  if (ret < 0 && ret != AVERROR(EAGAIN)) ocaml_avutil_raise_error(ret);
}

static inline AVFrame *resample_audio_frame(stream_t * stream, AVFrame * frame)
{
  AVCodecContext *enc_ctx = stream->codec_context;

  caml_release_runtime_system();

  if( ! stream->swr_ctx) {
    // create resampler context
    struct SwrContext *swr_ctx = swr_alloc();

    if (!swr_ctx) {
      caml_acquire_runtime_system();
      caml_raise_out_of_memory();
    }
    stream->swr_ctx = swr_ctx;

    // set options
    av_opt_set_channel_layout(swr_ctx, "in_channel_layout", frame->channel_layout, 0);
    av_opt_set_int(swr_ctx, "in_sample_rate", frame->sample_rate, 0);
    av_opt_set_sample_fmt(swr_ctx, "in_sample_fmt", (enum AVSampleFormat)frame->format, 0);
    av_opt_set_channel_layout(swr_ctx, "out_channel_layout", enc_ctx->channel_layout, 0);
    av_opt_set_int(swr_ctx, "out_sample_rate", enc_ctx->sample_rate, 0);
    av_opt_set_sample_fmt(swr_ctx, "out_sample_fmt", enc_ctx->sample_fmt, 0);

    // initialize the resampling context
    int ret = swr_init(swr_ctx);

    if (ret < 0) {
      caml_acquire_runtime_system();
      ocaml_avutil_raise_error(ret);
    }
  }

  int out_nb_samples = swr_get_out_samples(stream->swr_ctx, frame->nb_samples);

  if(! stream->sw_frame || out_nb_samples > stream->sw_frame->nb_samples) {
    if(stream->sw_frame) {
      av_frame_free(&stream->sw_frame);
    }

    // Allocate the resampler frame
    stream->sw_frame = av_frame_alloc();

    if(!stream->sw_frame) {
      caml_acquire_runtime_system();
      caml_raise_out_of_memory();
    }

    stream->sw_frame->nb_samples     = out_nb_samples;
    stream->sw_frame->channel_layout = enc_ctx->channel_layout;
    stream->sw_frame->format         = enc_ctx->sample_fmt;
    stream->sw_frame->sample_rate    = enc_ctx->sample_rate;

    int ret = av_frame_get_buffer(stream->sw_frame, 0);

    if (ret < 0) {
      caml_acquire_runtime_system();
      ocaml_avutil_raise_error(ret);
    }
  }
  
  int ret = av_frame_make_writable(stream->sw_frame);

  if (ret < 0) {
    caml_acquire_runtime_system();
    ocaml_avutil_raise_error(ret);
  }

  // convert to destination format
  ret = swr_convert(stream->swr_ctx,
                    stream->sw_frame->extended_data, out_nb_samples,
                    (const uint8_t **)frame->extended_data, frame->nb_samples);

  caml_acquire_runtime_system();

  if(ret < 0) ocaml_avutil_raise_error(ret);

  return stream->sw_frame;
}

static inline void write_audio_frame(av_t * av, int stream_index, AVFrame * frame)
{
  int err, fifo_size, frame_size;

  if(av->format_context->nb_streams == 0) {
    if( ! av->format_context->oformat) Fail("Invalid input found when writting audio frame");

    enum AVCodecID codec_id = av->format_context->oformat->audio_codec;

    caml_release_runtime_system();
    AVCodec * codec = avcodec_find_encoder(codec_id);
    caml_acquire_runtime_system();

    enum AVSampleFormat sample_format = (enum AVSampleFormat)frame->format;

    if(codec && codec->sample_fmts) {
      sample_format = codec->sample_fmts[0];
    }
      
    stream_t * stream = new_audio_stream(av, codec_id, frame->channel_layout,
                                         sample_format, 192000, frame->sample_rate,
                                         (AVRational){1, frame->sample_rate});
    if(!stream && !av->streams) caml_raise_out_of_memory();
  }

  stream_t * stream = av->streams[stream_index];

  if( ! stream->codec_context) Fail("Could not find stream index");

  AVCodecContext * enc_ctx = stream->codec_context;
  
  if(frame && (frame->sample_rate != enc_ctx->sample_rate
               || frame->channel_layout != enc_ctx->channel_layout
               || ((enum AVSampleFormat)frame->format) != enc_ctx->sample_fmt)) {

    frame = resample_audio_frame(stream, frame);
  }

  if(stream->audio_fifo == NULL) {

    if(frame) {
      frame->pts = stream->pts;
      stream->pts += frame->nb_samples;
    }
  
    write_frame(av, stream_index, enc_ctx, frame);
  }
  else {
    AVAudioFifo *fifo = stream->audio_fifo;
    AVFrame * output_frame = stream->enc_frame;

    caml_release_runtime_system();
  
    if(frame != NULL) {
      // Store the new samples in the FIFO buffer.
      err = av_audio_fifo_write(fifo, (void **)(const uint8_t**)frame->extended_data, frame->nb_samples);

      if (err < 0) {
        caml_acquire_runtime_system();
        ocaml_avutil_raise_error(err);
      }
    }

    fifo_size = av_audio_fifo_size(fifo);
    frame_size = enc_ctx->frame_size;

    for(; fifo_size >= frame_size || frame == NULL; fifo_size = av_audio_fifo_size(fifo)) {
      if(fifo_size > 0) {
        err = av_frame_make_writable(output_frame);
        if (err < 0) {
          caml_acquire_runtime_system();
          ocaml_avutil_raise_error(err);
        }

        int read_size = av_audio_fifo_read(fifo, (void **)output_frame->data, frame_size);

        if (frame && read_size < frame_size) {
          caml_acquire_runtime_system();
          Fail("Invalid input: read_size < frame_size");
        }

        output_frame->nb_samples = frame_size;
        output_frame->pts = stream->pts;
        stream->pts += frame_size;
      }
      else {
        output_frame = NULL;
      }

      caml_acquire_runtime_system(); 
      write_frame(av, stream_index, enc_ctx, output_frame);
      caml_release_runtime_system();

      if (fifo_size == 0) break;
    }
  }

  caml_acquire_runtime_system();
}

static void scale_video_frame(stream_t * stream, AVFrame * frame)
{
  AVCodecContext * enc_ctx = stream->codec_context;

  if( ! stream->sws_ctx) {
    // create scale context
    stream->sws_ctx = sws_getContext(frame->width, frame->height,
                                     (enum AVPixelFormat)frame->format,
                                     enc_ctx->width, enc_ctx->height,
                                     enc_ctx->pix_fmt,
                                     SWS_BICUBIC, NULL, NULL, NULL);
    if ( ! stream->sws_ctx) caml_raise_out_of_memory();

    // Allocate the scale frame
    stream->sw_frame = av_frame_alloc();
    if( ! stream->sw_frame) caml_raise_out_of_memory();

    stream->sw_frame->width = enc_ctx->width;
    stream->sw_frame->height = enc_ctx->height;
    stream->sw_frame->format = enc_ctx->pix_fmt;

    caml_release_runtime_system();
    int ret = av_frame_get_buffer(stream->sw_frame, 32);
    caml_acquire_runtime_system();
    if (ret < 0) ocaml_avutil_raise_error(ret);
  }
  
  caml_release_runtime_system();
  int ret = av_frame_make_writable(stream->sw_frame);
  caml_acquire_runtime_system();

  if (ret < 0) ocaml_avutil_raise_error(ret);

  // convert to destination format
  caml_release_runtime_system();
  sws_scale(stream->sws_ctx,
            (const uint8_t * const *)frame->data, frame->linesize,
            0, frame->height,
            stream->sw_frame->data, stream->sw_frame->linesize);
  caml_acquire_runtime_system();
}

static inline void write_video_frame(av_t * av, int stream_index, AVFrame * frame)
{
  if(av->format_context->nb_streams == 0) {
    if( ! av->format_context->oformat) Fail("Failed to create undefined output format");

    enum AVCodecID codec_id = av->format_context->oformat->video_codec;

    caml_release_runtime_system();
    AVCodec * codec = avcodec_find_encoder(codec_id);
    caml_acquire_runtime_system();

    enum AVPixelFormat pix_format = (enum AVPixelFormat)frame->format;

    if(codec && codec->pix_fmts) {
      pix_format = codec->pix_fmts[0];
    }
      
    new_video_stream(av, av->format_context->oformat->video_codec,
                     frame->width, frame->height, pix_format,
                     frame->width * frame->height * 4, 25,
                     (AVRational){1, 25});
  }

  if( ! av->streams) Fail("Failed to write in closed output");

  stream_t * stream = av->streams[stream_index];
  
  if( ! stream->codec_context) Fail("Failed to write video frame with no encoder");

  AVCodecContext * enc_ctx = stream->codec_context;
  
  if(frame && (frame->width != enc_ctx->width
               || frame->height != enc_ctx->height
               || ((enum AVPixelFormat)frame->format) != enc_ctx->pix_fmt)) {

    scale_video_frame(stream, frame);
  }

  if(frame) {
    frame->pts = stream->pts++;
  }
    
  write_frame(av, stream_index, enc_ctx, frame);
}

static inline void write_subtitle_frame(av_t * av, int stream_index, AVSubtitle * subtitle)
{
  stream_t * stream = av->streams[stream_index];
  AVStream * avstream = av->format_context->streams[stream->index];
  AVCodecContext * enc_ctx = stream->codec_context;
  
  if( ! stream->codec_context) Fail("Failed to write subtitle frame with no encoder");

  int err;
  int size = 512;
  AVPacket packet;
  av_init_packet(&packet);
  packet.data = NULL;
  packet.size = 0;
  
  caml_release_runtime_system();
  err = av_new_packet(&packet, size);

  if (err < 0) {
    caml_acquire_runtime_system();
    ocaml_avutil_raise_error(err);
  }

  err = avcodec_encode_subtitle(stream->codec_context, packet.data, packet.size, subtitle);

  if (err < 0) {
    av_packet_unref(&packet);
    caml_acquire_runtime_system();
    ocaml_avutil_raise_error(err);
  }

  packet.pts = subtitle->pts;
  packet.duration = subtitle->end_display_time - subtitle->pts;
  packet.dts = subtitle->pts;
  av_packet_rescale_ts(&packet, enc_ctx->time_base, avstream->time_base);

  packet.stream_index = stream_index;
  packet.pos = -1;

  err = av_interleaved_write_frame(av->format_context, &packet);

  av_packet_unref(&packet);
  caml_acquire_runtime_system();

  if(err < 0) ocaml_avutil_raise_error(err);
}

CAMLprim value ocaml_av_write_stream_frame(value _stream, value _frame)
{
  CAMLparam2(_stream, _frame);
  av_t * av = StreamAv_val(_stream);
  int index = StreamIndex_val(_stream);
  
  if(!av->streams) Fail("Invalid input: no streams provided");

  enum AVMediaType type = av->streams[index]->codec_context->codec_type;

  if(type == AVMEDIA_TYPE_AUDIO) {
    write_audio_frame(av, index, Frame_val(_frame));
  }
  else if(type == AVMEDIA_TYPE_VIDEO) {
    write_video_frame(av, index, Frame_val(_frame));
  }
  else if(type == AVMEDIA_TYPE_SUBTITLE) {
    write_subtitle_frame(av, index, Subtitle_val(_frame));
  }

  CAMLreturn(Val_unit);
}

CAMLprim value ocaml_av_write_audio_frame(value _av, value _frame) {
  CAMLparam2(_av, _frame);
  av_t * av = Av_val(_av);
  AVFrame * frame = Frame_val(_frame);

  write_audio_frame(av, 0, frame);

  CAMLreturn(Val_unit);
}

CAMLprim value ocaml_av_write_video_frame(value _av, value _frame) {
  CAMLparam2(_av, _frame);
  av_t * av = Av_val(_av);
  AVFrame * frame = Frame_val(_frame);

  write_video_frame(av, 0, frame);

  CAMLreturn(Val_unit);
}


CAMLprim value ocaml_av_close(value _av)
{
  CAMLparam1(_av);
  av_t * av = Av_val(_av);
  
  if( ! av->is_input && av->streams) {
    // flush encoders of the output file
    unsigned int i;
    for(i = 0; i < av->format_context->nb_streams; i++) {

      AVCodecContext * enc_ctx = av->streams[i]->codec_context;

      if( ! enc_ctx) continue;
  
      if(enc_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
        write_audio_frame(av, i, NULL);
      }
      else if(enc_ctx->codec_type == AVMEDIA_TYPE_VIDEO) {
        write_video_frame(av, i, NULL);
      }
    }

    // write the trailer
    caml_release_runtime_system();
    av_write_trailer(av->format_context);
    caml_acquire_runtime_system();
  }

  caml_register_generational_global_root(&_av);
  caml_release_runtime_system();
  close_av(av);
  caml_acquire_runtime_system();
  caml_remove_generational_global_root(&_av);

  CAMLreturn(Val_unit);
}
