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
  value packet_value;
  value frame_value;
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
  value packet_value;
  int end_of_file;
  int selected_streams;
  stream_t * best_audio_stream;
  stream_t * best_video_stream;
  stream_t * best_subtitle_stream;

  // output
  int header_written;
  int release_out;
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

  if(stream->packet_value) caml_remove_generational_global_root(&stream->packet_value);

  if(stream->frame_value) caml_remove_generational_global_root(&stream->frame_value);

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
      if( ! (av->format_context->oformat->flags & AVFMT_NOFILE))
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
  if( ! av) return;

  close_av(av);
  
  if(av->packet_value) caml_remove_generational_global_root(&av->packet_value);

  if(av->control_message_callback) {
    caml_remove_generational_global_root(&av->control_message_callback);
  }

  free(av);
}

static void finalize_av(value v)
{
  free_av(Av_val(v));
}

static struct custom_operations av_ops = {
  "ocaml_av_context",
  finalize_av,
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

CAMLprim value ocaml_av_create_io(value bufsize, value cb) {
  CAMLparam1(cb);
  CAMLlocal1(ret);

  avio_t *avio = (avio_t *)calloc(1, sizeof(avio_t));
  if (!avio) caml_raise_out_of_memory();

  avio->format_context = avformat_alloc_context();
  if (!avio->format_context) {
    free(avio);
    caml_raise_out_of_memory();
  }

  avio->buffer_size = Int_val(bufsize);
  avio->buffer = av_malloc(avio->buffer_size);
  if (!avio->buffer) {
    av_freep(avio->format_context);
    free(avio);
    caml_raise_out_of_memory();
  }

  
  if (Field(cb,1) != Val_none) {
    avio->seek_cb = Some_val(Field(cb,1));
    caml_register_generational_global_root(&avio->seek_cb);
    avio->avio_context = avio_alloc_context(
      avio->buffer, avio->buffer_size, 0, (void *)avio,
      ocaml_avio_read_callback, NULL,
      ocaml_avio_seek_callback);
  } else {
    avio->seek_cb = (value)NULL;
    avio->avio_context = avio_alloc_context(
      avio->buffer, avio->buffer_size, 0, (void *)avio,
      ocaml_avio_read_callback, NULL, NULL);
  }

  if (!avio->avio_context) {
    av_freep(avio->buffer);
    av_freep(avio->format_context);
    free(avio);
    caml_raise_out_of_memory();
  }

  avio->format_context->pb = avio->avio_context;

  avio->read_cb = Field(cb,0);
  caml_register_generational_global_root(&avio->read_cb);

  ret = caml_alloc_custom(&avio_ops, sizeof(avio_t*), 0, 1);

  Avio_val(ret) = avio;

  CAMLreturn(ret);
}

CAMLprim value caml_av_input_io_finalise(value _avio) {
  CAMLparam1(_avio);
  // format_context and the buffer are freed as part of av_close.
  avio_t *avio = Avio_val(_avio);
  av_freep(avio->avio_context);
  caml_remove_generational_global_root(&avio->read_cb);
  if (avio->seek_cb)
    caml_remove_generational_global_root(&avio->seek_cb);
  free(avio);
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
  if( ! inputFormat) Raise(EXN_FAILURE, "Empty input format");

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
  av_t *av = (av_t*)calloc(1, sizeof(av_t));
  if ( ! av) Fail("Failed to allocate input context");

  av->is_input = 1;
  av->release_out = 1;
  av->format_context = format_context;
  
  int ret = avformat_open_input(&av->format_context, url, format, NULL);
  
  if (ret < 0 || ! av->format_context) {
    free(av);
    Fail("Failed to open input %s", format ? format->name : url);
  }

  // retrieve stream information
  ret = avformat_find_stream_info(av->format_context, NULL);

  if (ret < 0) {
    free(av);
    Fail("Stream information not found");
  }
  return av;
}

CAMLprim value ocaml_av_open_input(value _url)
{
  CAMLparam1(_url);
  CAMLlocal1(ans);
  char * url = strndup(String_val(_url), caml_string_length(_url));

  // open input url
  caml_release_runtime_system();
  av_t *av = open_input(url, NULL, NULL);

  free(url);
  caml_acquire_runtime_system();
  if( ! av) Raise(EXN_FAILURE, "%s", ocaml_av_error_msg);

  // allocate format context
  ans = caml_alloc_custom(&av_ops, sizeof(av_t*), 0, 1);
  Av_val(ans) = av;

  CAMLreturn(ans);
}

CAMLprim value ocaml_av_open_input_format(value _format)
{
  CAMLparam1(_format);
  CAMLlocal1(ans);
  AVInputFormat *format = InputFormat_val(_format);

  // open input format
  caml_release_runtime_system();
  av_t *av = open_input(NULL, format, NULL);
  caml_acquire_runtime_system();
  if( ! av) Raise(EXN_FAILURE, "%s", ocaml_av_error_msg);

  // allocate format context
  ans = caml_alloc_custom(&av_ops, sizeof(av_t*), 0, 1);
  Av_val(ans) = av;

  CAMLreturn(ans);
}

CAMLprim value ocaml_av_open_input_stream(value _avio)
{
  CAMLparam1(_avio);
  CAMLlocal1(ans);
  avio_t *avio = Avio_val(_avio);

  // open input format
  caml_release_runtime_system();
  av_t *av = open_input(NULL, NULL, avio->format_context);
  caml_acquire_runtime_system();
  if( ! av) {
    avio->format_context = NULL;
    Raise(EXN_FAILURE, "%s", ocaml_av_error_msg);
  }

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

  while((tag = av_dict_get(metadata, "", tag, AV_DICT_IGNORE_SUFFIX))) {

    pair = caml_alloc_tuple(2);
    Store_field(pair, 0, caml_copy_string(tag->key));
    Store_field(pair, 1, caml_copy_string(tag->value));

    List_add(list, cons, pair);
  }

  CAMLreturn(list);
}

CAMLprim value ocaml_av_get_duration(value _av, value _stream_index, value _time_format)
{
  CAMLparam2(_av, _time_format);
  CAMLlocal1(ans);
  av_t * av = Av_val(_av);
  int index = Int_val(_stream_index);
  
  if( ! av->format_context) Raise(EXN_FAILURE, "Failed to get closed input duration");

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

static value provide_packet_value(value * packet_value)
{
  // Allocate the packet if needed
  if( ! *packet_value) {
    if( ! alloc_packet_value(packet_value)) Raise(EXN_FAILURE, "Failed to allocate packet value");
    caml_register_generational_global_root(packet_value);
  }
  return *packet_value;
}

static stream_t** allocate_input_context(av_t *av)
{
  if( ! av->format_context) Fail("Failed to read closed input");
  
  // Allocate streams context array
  av->streams = (stream_t**)calloc(av->format_context->nb_streams, sizeof(stream_t*));
  if( ! av->streams) Fail("Failed to allocate streams array");

  return av->streams;
}

static AVFrame * allocate_type_frame(enum AVMediaType type, value * pvalue)
{
  if(type == AVMEDIA_TYPE_SUBTITLE) {
    return (AVFrame *)alloc_subtitle_value(pvalue);
  }
  else {
    return alloc_frame_value(pvalue);
  }
}

static AVFrame * provide_stream_frame(av_t * av, stream_t * stream, value * frame_value)
{
  if(av->release_out) {
    return allocate_type_frame(stream->codec_context->codec_type, frame_value);
  }
  else {
    // Allocate the frame if needed
    if(stream->frame_value) {
      *frame_value = stream->frame_value;
    }
    else {
      if( ! allocate_type_frame(stream->codec_context->codec_type, frame_value))
        return NULL;

      stream->frame_value = *frame_value;
      caml_register_generational_global_root(&stream->frame_value);
    }
    return Frame_val(*frame_value);
  }
}

static stream_t * allocate_stream_context(av_t *av, int index, AVCodec *codec)
{
  enum AVMediaType type = codec->type;

  if(type != AVMEDIA_TYPE_AUDIO && type != AVMEDIA_TYPE_VIDEO && type != AVMEDIA_TYPE_SUBTITLE) Fail("Failed to allocate stream %d of media type %s", index, av_get_media_type_string(type));

  stream_t * stream = (stream_t*)calloc(1, sizeof(stream_t));
  if( ! stream) Fail("Failed to allocate stream context");

  stream->index = index;
  av->streams[index] = stream;

  stream->codec_context = avcodec_alloc_context3(codec);

  if( ! stream->codec_context) Fail("Failed to allocate stream %d codec context", index);

  return stream;
}

static stream_t * open_stream_index(av_t *av, int index)
{
  if( ! av->format_context) Fail("Failed to open stream %d of closed input", index);

  if(index < 0 || index >= av->format_context->nb_streams) Fail("Failed to open stream %d : index out of bounds", index);
  
  if( ! av->streams && ! allocate_input_context(av)) return NULL;

  // find decoder for the stream
  AVCodecParameters *dec_param = av->format_context->streams[index]->codecpar;

  AVCodec *dec = avcodec_find_decoder(dec_param->codec_id);
  if( ! dec) Fail("Failed to find stream %d %s decoder", index, avcodec_get_name(dec_param->codec_id));
  
  stream_t * stream = allocate_stream_context(av, index, dec);
  if ( ! stream) return NULL;

  // initialize the stream parameters with demuxer information
  int ret = avcodec_parameters_to_context(stream->codec_context, dec_param);
  if(ret < 0) Fail("Failed to initialize the stream context with the stream parameters : %s", av_err2str(ret));

  // Open the decoder
  ret = avcodec_open2(stream->codec_context, dec, NULL);
  if(ret < 0) Fail("Failed to open stream %d codec : %s", index, av_err2str(ret));

  return stream;
}

#define Check_stream(av, index) {                               \
    if( ! (av)->streams || ! (av)->streams[(index)]) {          \
      caml_release_runtime_system();                            \
      stream_t * stream = open_stream_index((av), (index));     \
      caml_acquire_runtime_system();                            \
      if( ! stream) Raise(EXN_FAILURE, "%s", ocaml_av_error_msg);     \
    }                                                           \
  }

CAMLprim value ocaml_av_find_best_stream(value _av, value _media_type)
{
  CAMLparam2(_av, _media_type);
  av_t * av = Av_val(_av);
  enum AVMediaType type = MediaType_val(_media_type);

  int index = av_find_best_stream(av->format_context, type, -1, -1, NULL, 0);
  if(index < 0) Raise(EXN_FAILURE, "Failed to find %s stream", av_get_media_type_string(type));

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


static value decode_packet(av_t * av, stream_t * stream, AVPacket * packet, AVFrame * frame)
{
  AVCodecContext * dec = stream->codec_context;
  value frame_kind = 0;
  int ret = AVERROR_INVALIDDATA;

  if(dec->codec_type == AVMEDIA_TYPE_AUDIO ||
     dec->codec_type == AVMEDIA_TYPE_VIDEO) {
  
    ret = avcodec_send_packet(dec, packet);
  
    if (ret >= 0 || ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
  
      if (ret >= 0) packet->size = 0;
  
      // decode frame
      ret = avcodec_receive_frame(dec, frame);
    }
  
    if(dec->codec_type == AVMEDIA_TYPE_AUDIO) {
      frame_kind = PVV_Audio;
    }
    else {
      frame_kind = PVV_Video;
    }
  }
  else if(dec->codec_type == AVMEDIA_TYPE_SUBTITLE) {
  
    ret = avcodec_decode_subtitle2(dec, (AVSubtitle *)frame, &stream->got_frame, packet);
  
    if (ret >= 0) packet->size = 0;
  
    frame_kind = PVV_Subtitle;
  }
  
  if(packet->size <= 0) {
    av_packet_unref(packet);
  }
  
  if(ret >= 0) {
    stream->got_frame = 1;
  }
  else if (ret == AVERROR_EOF) {
    stream->got_frame = 0;
    frame_kind = PVV_End_of_file;
  }
  else if (ret == AVERROR(EAGAIN)) {
    frame_kind = 0;
  }
  else {
    Log("Failed to decode %s frame : %s", av_get_media_type_string(dec->codec_type), av_err2str(ret));
    frame_kind = PVV_Error;
  }

  return frame_kind;
}

static void read_packet(av_t * av, AVPacket * packet, int stream_index, stream_t ** selected_streams)
{
  for(;;) {
    // read frames from the file
    int ret = av_read_frame(av->format_context, packet);

    if(ret < 0) {
      packet->data = NULL;
      packet->size = 0;
      av->end_of_file = 1;
      break;
    }

    if(packet->stream_index == stream_index
       || (stream_index < 0
           && ( ! selected_streams
                || selected_streams[packet->stream_index]))) break;

    av_packet_unref(packet);
  }
}


CAMLprim value ocaml_av_read_stream_packet(value _stream) {
  CAMLparam1(_stream);
  CAMLlocal2(ans, packet_value);
  av_t * av = StreamAv_val(_stream);
  int index = StreamIndex_val(_stream);

  Check_stream(av, index);

  if(av->release_out) {
    if( ! alloc_packet_value(&packet_value)) Raise(EXN_FAILURE, "%s", ocaml_av_error_msg);
  }
  else {
    packet_value = provide_packet_value(&av->streams[index]->packet_value);
  }
  AVPacket * packet = Packet_val(packet_value);

  caml_release_runtime_system();
  read_packet(av, packet, index, NULL);
  caml_acquire_runtime_system();

  if(av->end_of_file) {
    ans = PVV_End_of_file;
  }
  else {
    ans = caml_alloc_tuple(2);
    Field(ans, 0) = PVV_Packet;
    Field(ans, 1) = packet_value;
  }
  CAMLreturn(ans);
}


CAMLprim value ocaml_av_read_stream_frame(value _stream) {
  CAMLparam1(_stream);
  CAMLlocal2(ans, frame_value);
  av_t * av = StreamAv_val(_stream);
  int index = StreamIndex_val(_stream);

  Check_stream(av, index);

  stream_t * stream = av->streams[index];
  AVPacket * packet = Packet_val(provide_packet_value(&stream->packet_value));
  value frame_kind = 0;

  AVFrame * frame = provide_stream_frame(av, stream, &frame_value);
  if( ! frame) Raise(EXN_FAILURE, "%s", ocaml_av_error_msg);

  caml_release_runtime_system();

  for(; ! frame_kind;) {
    if( ! av->end_of_file && packet->size <= 0) {
      read_packet(av, packet, index, NULL);
    }
    frame_kind = decode_packet(av, stream, packet, frame);
  }
  caml_acquire_runtime_system();

  if(frame_kind == PVV_Error) Raise(EXN_FAILURE, "%s", ocaml_av_error_msg);

  if(frame_kind == PVV_End_of_file) {
    ans = PVV_End_of_file;
  }
  else {
    ans = caml_alloc_tuple(2);
    Field(ans, 0) = PVV_Frame;
    Field(ans, 1) = frame_value;
  }

  CAMLreturn(ans);
}


CAMLprim value ocaml_av_read_input_packet(value _av) {
  CAMLparam1(_av);
  CAMLlocal3(ans, stream_packet, packet_value);
  av_t * av = Av_val(_av);
  stream_t ** selected_streams = av->selected_streams ? av->streams : NULL;

  if(! av->streams && ! allocate_input_context(av)) Raise(EXN_FAILURE, "%s", ocaml_av_error_msg);

  if(av->release_out) {
    if( ! alloc_packet_value(&packet_value)) Raise(EXN_FAILURE, "%s", ocaml_av_error_msg);
  }
  else {
    packet_value = provide_packet_value(&av->packet_value);
  }
  AVPacket * packet = Packet_val(packet_value);

  caml_release_runtime_system();
  read_packet(av, packet, -1, selected_streams);
  caml_acquire_runtime_system();

  if(av->end_of_file) {
    ans = PVV_End_of_file;
  }
  else {
    int index = packet->stream_index;

    Check_stream(av, index);

    stream_packet = caml_alloc_tuple(2);
    Field(stream_packet, 0) = Val_int(index);
    Field(stream_packet, 1) = packet_value;

    ans = caml_alloc_tuple(2);
    switch(av->streams[index]->codec_context->codec_type) {
    case AVMEDIA_TYPE_AUDIO : Field(ans, 0) = PVV_Audio; break;
    case AVMEDIA_TYPE_VIDEO : Field(ans, 0) = PVV_Video; break;
    default : Field(ans, 0) = PVV_Subtitle; break;
    }
    Field(ans, 1) = stream_packet;
  }
  CAMLreturn(ans);
}


CAMLprim value ocaml_av_read_input_frame(value _av)
{
  CAMLparam1(_av);
  CAMLlocal3(ans, stream_frame, frame_value);
  av_t * av = Av_val(_av);

  if(! av->streams && ! allocate_input_context(av)) Raise(EXN_FAILURE, "%s", ocaml_av_error_msg);

  AVPacket * packet = Packet_val(provide_packet_value(&av->packet_value));
  stream_t ** streams = av->streams;
  stream_t ** selected_streams = av->selected_streams ? av->streams : NULL;
  unsigned int nb_streams = av->format_context->nb_streams;
  stream_t * stream = NULL;
  value frame_kind = 0;

  caml_release_runtime_system();

  for(; ! frame_kind;) {
    if( ! av->end_of_file) {
  
      if(packet->size <= 0) {
        read_packet(av, packet, -1, selected_streams);
        if(av->end_of_file) continue;
      }
  
      if((stream = streams[packet->stream_index]) == NULL) {
        if(NULL == (stream = open_stream_index(av, packet->stream_index))) {
          frame_kind = PVV_Error;
          break;
        }
      }
    }
    else {
      // If the end of file is reached, iteration on the streams to find one to flush
      unsigned int i = 0;
      for(; i < nb_streams; i++) {
        if((stream = streams[i]) && stream->got_frame) break;
      }

      if(i == nb_streams) {
        frame_kind = PVV_End_of_file;
        break;
      }
    }

    caml_acquire_runtime_system();

    AVFrame * frame = allocate_type_frame(stream->codec_context->codec_type, &frame_value);
    if( ! frame) Raise(EXN_FAILURE, "%s", ocaml_av_error_msg);

    caml_release_runtime_system();

    frame_kind = decode_packet(av, stream, packet, frame);
  }

  caml_acquire_runtime_system();
  if(frame_kind == PVV_Error) Raise(EXN_FAILURE, "%s", ocaml_av_error_msg);
  
  if(frame_kind == PVV_End_of_file) {
    ans = PVV_End_of_file;
  }
  else {
    stream_frame = caml_alloc_tuple(2);
    Field(stream_frame, 0) = Val_int(stream->index);
    Field(stream_frame, 1) = frame_value;

    ans = caml_alloc_tuple(2);
    Field(ans, 0) = frame_kind;
    Field(ans, 1) = stream_frame;
  }
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

  if( ! av->format_context) Raise(EXN_FAILURE, "Failed to seek closed input");
  
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

  if(ret < 0) Raise(EXN_FAILURE, "Av seek frame failed : %s", av_err2str(ret));

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

void value_of_outputFormat(AVOutputFormat *outputFormat, value * p_value)
{
  if( ! outputFormat) Raise(EXN_FAILURE, "Empty output format");

  *p_value = caml_alloc_custom(&outputFormat_ops, sizeof(AVOutputFormat*), 0, 1);
  OutputFormat_val((*p_value)) = outputFormat;
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


static av_t * open_output(AVOutputFormat *format, const char *format_name, const char *file_name)
{
  av_t *av = (av_t*)calloc(1, sizeof(av_t));
  if ( ! av) Fail("Failed to allocate output context");

  avformat_alloc_output_context2(&av->format_context, format, format_name, file_name);
  
  if ( ! av->format_context) {
    free_av(av);
    Fail("Failed to allocate format context");
  }

  // open the output file, if needed
  if( ! (av->format_context->oformat->flags & AVFMT_NOFILE)) {
    int ret = avio_open(&av->format_context->pb, file_name, AVIO_FLAG_WRITE);
    if (ret < 0) {
      free_av(av);
      Fail("Failed to open output file : %s\n", av_err2str(ret));
    }
  }
  return av;
}

CAMLprim value ocaml_av_open_output(value _filename)
{
  CAMLparam1(_filename);
  CAMLlocal1(ans);
  char * filename = strndup(String_val(_filename), caml_string_length(_filename));

  // open output file
  caml_release_runtime_system();
  av_t *av = open_output(NULL, NULL, filename);

  free(filename);
  caml_acquire_runtime_system();
  if( ! av) Raise(EXN_FAILURE, "%s", ocaml_av_error_msg);

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
  caml_release_runtime_system();
  av_t *av = open_output(format, NULL, NULL);
  caml_acquire_runtime_system();
  if( ! av) Raise(EXN_FAILURE, "%s", ocaml_av_error_msg);

  // allocate format context
  ans = caml_alloc_custom(&av_ops, sizeof(av_t*), 0, 1);
  Av_val(ans) = av;

  CAMLreturn(ans);
}

CAMLprim value ocaml_av_open_output_format_name(value _format_name)
{
  CAMLparam1(_format_name);
  CAMLlocal1(ans);
  char * format_name = strndup(String_val(_format_name), caml_string_length(_format_name));

  // open output file
  caml_release_runtime_system();
  av_t *av = open_output(NULL, format_name, NULL);

  free(format_name);
  caml_acquire_runtime_system();
  if( ! av) Raise(EXN_FAILURE, "%s", ocaml_av_error_msg);

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

  if( ! av->format_context) Raise(EXN_FAILURE, "Failed to set metadata to closed output");
  if(av->header_written) Raise(EXN_FAILURE, "Failed to set metadata : header already written");

  int i, ret, len = Wosize_val(_tags);
  
  for(i = 0; i < len; i++) {

    pair = Field(_tags, i);

    ret = av_dict_set(&metadata,
                      String_val(Field(pair, 0)),
                      String_val(Field(pair, 1)),
                      0);
    if (ret < 0) Raise(EXN_FAILURE, "Failed to set metadata : %s", av_err2str(ret));
  }

  if(index < 0) {
    av->format_context->metadata = metadata;
  }
  else {
    av->format_context->streams[index]->metadata = metadata;
  }

  CAMLreturn(Val_unit);
}

static stream_t * new_stream(av_t *av, enum AVCodecID codec_id)
{
  if( ! av->format_context) Fail("Failed to add stream to closed output");
  if(av->header_written) Fail("Failed to create new stream : header already written");

  AVCodec * encoder = avcodec_find_encoder(codec_id);
  if( ! encoder) Fail("Failed to find %s encoder", avcodec_get_name(codec_id));

  // Allocate streams array
  size_t streams_size = sizeof(stream_t*) * (av->format_context->nb_streams + 1);
  stream_t**streams = (stream_t**)realloc(av->streams, streams_size);
  if( ! streams) Fail("Failed to allocate streams array");

  streams[av->format_context->nb_streams] = NULL;
  av->streams = streams;

  stream_t * stream = allocate_stream_context(av, av->format_context->nb_streams, encoder);
  if ( ! stream) return NULL;

  AVStream * avstream = avformat_new_stream(av->format_context, NULL);
  if( ! avstream) Fail("Failed to allocate output stream");

  avstream->id = av->format_context->nb_streams - 1;

  return stream;
}

static stream_t * init_stream_encoder(av_t *av, stream_t *stream)
{
  AVCodecContext * enc_ctx = stream->codec_context;

  // Some formats want stream headers to be separate.
  if (av->format_context->oformat->flags & AVFMT_GLOBALHEADER)
    enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

  int ret = avcodec_open2(enc_ctx, NULL, NULL);
  if (ret < 0) Fail("Failed to open stream %d encoder : %s", stream->index, av_err2str(ret));

  AVStream * avstream = av->format_context->streams[stream->index];
  avstream->time_base = enc_ctx->time_base;

  ret = avcodec_parameters_from_context(avstream->codecpar, enc_ctx);
  if (ret < 0) Fail("Failed to copy the stream parameters");

  return stream;
}

static stream_t * new_audio_stream(av_t *av, enum AVCodecID codec_id, uint64_t channel_layout, enum AVSampleFormat sample_fmt, int bit_rate, int sample_rate, AVRational time_base)
{
  stream_t * stream = new_stream(av, codec_id);
  if( ! stream) return NULL;
  
  AVCodecContext * enc_ctx = stream->codec_context;

  enc_ctx->bit_rate = bit_rate;
  enc_ctx->sample_rate = sample_rate;
  enc_ctx->channel_layout = channel_layout;
  enc_ctx->channels = av_get_channel_layout_nb_channels(channel_layout);
  enc_ctx->sample_fmt = sample_fmt;
  enc_ctx->time_base = time_base;

  stream = init_stream_encoder(av, stream);
  if( ! stream) return NULL;

  if (enc_ctx->frame_size > 0
      || ! (enc_ctx->codec->capabilities & AV_CODEC_CAP_VARIABLE_FRAME_SIZE)) {

    // Allocate the buffer frame and audio fifo if the codec doesn't support variable frame size
    stream->enc_frame = av_frame_alloc();
    if( ! stream->enc_frame) Fail("Failed to allocate encoder frame");

    stream->enc_frame->nb_samples     = enc_ctx->frame_size;
    stream->enc_frame->channel_layout = enc_ctx->channel_layout;
    stream->enc_frame->format         = enc_ctx->sample_fmt;
    stream->enc_frame->sample_rate    = enc_ctx->sample_rate;

    int ret = av_frame_get_buffer(stream->enc_frame, 0);
    if (ret < 0) Fail("Failed to allocate encoder frame samples : %s)", av_err2str(ret));
  
    // Create the FIFO buffer based on the specified output sample format.
    stream->audio_fifo = av_audio_fifo_alloc(enc_ctx->sample_fmt, enc_ctx->channels, 1);
    if( ! stream->audio_fifo) Fail("Failed to allocate audio FIFO");
  }

  return stream;
}

CAMLprim value ocaml_av_new_audio_stream(value _av, value _audio_codec_id, value _channel_layout, value _sample_fmt, value _bit_rate, value _sample_rate, value _time_base)
{
  CAMLparam5(_av, _audio_codec_id, _channel_layout, _sample_fmt, _time_base);

  caml_release_runtime_system();
  stream_t * stream = new_audio_stream(Av_val(_av),
                                       AudioCodecID_val(_audio_codec_id),
                                       ChannelLayout_val(_channel_layout),
                                       SampleFormat_val(_sample_fmt),
                                       Int_val(_bit_rate),
                                       Int_val(_sample_rate),
                                       rational_of_value(_time_base));
  caml_acquire_runtime_system();

  if( ! stream) Raise(EXN_FAILURE, "%s", ocaml_av_error_msg);
                                       
  CAMLreturn(Val_int(stream->index));
}

CAMLprim value ocaml_av_new_audio_stream_byte(value *argv, int argn)
{
  return ocaml_av_new_audio_stream(argv[0], argv[1], argv[2], argv[3], argv[4], argv[5], argv[6]);
}


static stream_t * new_video_stream(av_t *av, enum AVCodecID codec_id, int width, int height, enum AVPixelFormat pix_fmt, int bit_rate, int frame_rate, AVRational time_base)
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
 
  stream = init_stream_encoder(av, stream);
  return stream;
}

CAMLprim value ocaml_av_new_video_stream(value _av, value _video_codec_id, value _width, value _height, value _pix_fmt, value _bit_rate, value _frame_rate, value _time_base)
{
  CAMLparam4(_av, _video_codec_id, _pix_fmt, _time_base);

  caml_release_runtime_system();
  stream_t * stream = new_video_stream(
                                       Av_val(_av),
                                       VideoCodecID_val(_video_codec_id),
                                       Int_val(_width),
                                       Int_val(_height),
                                       PixelFormat_val(_pix_fmt),
                                       Int_val(_bit_rate),
                                       Int_val(_frame_rate),
                                       rational_of_value(_time_base));
  caml_acquire_runtime_system();

  if( ! stream) Raise(EXN_FAILURE, "%s", ocaml_av_error_msg);

  CAMLreturn(Val_int(stream->index));
}

CAMLprim value ocaml_av_new_video_stream_byte(value *argv, int argn)
{
  return ocaml_av_new_video_stream(argv[0], argv[1], argv[2], argv[3], argv[4], argv[5], argv[6], argv[7]);
}


static stream_t * new_subtitle_stream(av_t *av, enum AVCodecID codec_id, AVRational time_base)
{
  stream_t * stream = new_stream(av, codec_id);
  if( ! stream) return NULL;

  int ret = subtitle_header_default(stream->codec_context);
  if (ret < 0) Fail("Failed to create new subtitle stream : %s", av_err2str(ret));

  stream->codec_context->time_base = time_base;

  stream = init_stream_encoder(av, stream);
  return stream;
}

CAMLprim value ocaml_av_new_subtitle_stream(value _av, value _subtitle_codec_id, value _time_base)
{
  CAMLparam3(_av, _subtitle_codec_id, _time_base);

  caml_release_runtime_system();
  stream_t * stream = new_subtitle_stream(Av_val(_av),
                                          SubtitleCodecID_val(_subtitle_codec_id),
                                          rational_of_value(_time_base));
  caml_acquire_runtime_system();

  if( ! stream) Raise(EXN_FAILURE, "%s", ocaml_av_error_msg);

  CAMLreturn(Val_int(stream->index));
}

CAMLprim value ocaml_av_write_stream_packet(value _stream, value _packet)
{
  CAMLparam2(_stream, _packet);
  av_t * av = StreamAv_val(_stream);
  int ret = 0, stream_index = StreamIndex_val(_stream);
  AVPacket *packet = Packet_val(_packet);

  if( ! av->streams) Raise(EXN_FAILURE, "Failed to write in closed output");

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

  if(ret < 0) Raise(EXN_FAILURE, "Failed to write packet : %s", av_err2str(ret));
    
  CAMLreturn(Val_unit);
}

static int write_frame(av_t * av, int stream_index, AVCodecContext * enc_ctx, AVFrame * frame)
{
  AVStream * avstream = av->format_context->streams[stream_index];

  if( ! av->header_written) {
    // write output file header
    int ret = avformat_write_header(av->format_context, NULL);
    if(ret < 0) {
      Log("Failed to write header : %s", av_err2str(ret));
      return ret;
    }

    av->header_written = 1;
  }

  AVPacket packet;
  av_init_packet(&packet);
  packet.data = NULL;
  packet.size = 0;

  // send the frame for encoding
  int ret = avcodec_send_frame(enc_ctx, frame);
  if(ret < 0) Log("Failed to send the frame to the encoder : %s", av_err2str(ret));

  // read all the available output packets (in general there may be any number of them
  while (ret >= 0) {
    ret = avcodec_receive_packet(enc_ctx, &packet);
 
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;

    if (ret < 0) {
      Log("Failed to encode frame : %s", av_err2str(ret));
      break;
    }

    packet.stream_index = stream_index;
    packet.pos = -1;
    av_packet_rescale_ts(&packet, enc_ctx->time_base, avstream->time_base);

    ret = av_interleaved_write_frame(av->format_context, &packet);
    if(ret < 0) Log("Failed to write frame : %s", av_err2str(ret));

    av_packet_unref(&packet);
  }
  return ret;
}

static AVFrame * resample_audio_frame(stream_t * stream, AVFrame * frame)
{
  AVCodecContext * enc_ctx = stream->codec_context;

  if( ! stream->swr_ctx) {
    // create resampler context
    struct SwrContext *swr_ctx = swr_alloc();
    if ( ! swr_ctx) Fail("Failed to allocate resampler context");
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
    if (ret < 0) Fail("Failed to encode frame : %s", av_err2str(ret));
  }

  int out_nb_samples = swr_get_out_samples(stream->swr_ctx, frame->nb_samples);

  if( ! stream->sw_frame || out_nb_samples > stream->sw_frame->nb_samples) {

    if(stream->sw_frame) {
      av_frame_free(&stream->sw_frame);
    }
    // Allocate the resampler frame
    stream->sw_frame = av_frame_alloc();
    if( ! stream->sw_frame) Fail("Failed to allocate resampler frame");

    stream->sw_frame->nb_samples     = out_nb_samples;
    stream->sw_frame->channel_layout = enc_ctx->channel_layout;
    stream->sw_frame->format         = enc_ctx->sample_fmt;
    stream->sw_frame->sample_rate    = enc_ctx->sample_rate;

    int ret = av_frame_get_buffer(stream->sw_frame, 0);
    if (ret < 0) Fail("Failed to allocate resampler frame samples : %s)", av_err2str(ret));
  }
  
  int ret = av_frame_make_writable(stream->sw_frame);
  if (ret < 0) Fail("Failed to make resampler frame writable : %s", av_err2str(ret));

  // convert to destination format
  ret = swr_convert(stream->swr_ctx,
                    stream->sw_frame->extended_data, out_nb_samples,
                    (const uint8_t **)frame->extended_data, frame->nb_samples);
  if(ret < 0) Fail("Failed to convert input samples : %s", av_err2str(ret));

  return stream->sw_frame;
}

static stream_t * write_audio_frame(av_t * av, int stream_index, AVFrame * frame)
{
  int ret;

  if(av->format_context->nb_streams == 0) {
    if( ! av->format_context->oformat) Fail("Failed to create undefined output format");

    enum AVCodecID codec_id = av->format_context->oformat->audio_codec;
    AVCodec * codec = avcodec_find_encoder(codec_id);
    enum AVSampleFormat sample_format = (enum AVSampleFormat)frame->format;

    if(codec && codec->sample_fmts) {
      sample_format = codec->sample_fmts[0];
    }
      
    stream_t * stream = new_audio_stream(av, codec_id, frame->channel_layout,
                                         sample_format, 192000, frame->sample_rate,
                                         (AVRational){1, frame->sample_rate});
    if( ! stream) return NULL;
  }

  if( ! av->streams) Fail("Failed to write in closed output");

  stream_t * stream = av->streams[stream_index];

  if( ! stream->codec_context) Fail("Failed to write audio frame with no encoder");

  AVCodecContext * enc_ctx = stream->codec_context;
  
  if(frame && (frame->sample_rate != enc_ctx->sample_rate
               || frame->channel_layout != enc_ctx->channel_layout
               || ((enum AVSampleFormat)frame->format) != enc_ctx->sample_fmt)) {

    frame = resample_audio_frame(stream, frame);
    if( ! frame) return NULL;
  }

  if(stream->audio_fifo == NULL) {

    if(frame) {
      frame->pts = stream->pts;
      stream->pts += frame->nb_samples;
    }
  
    ret = write_frame(av, stream_index, enc_ctx, frame);
    if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) return NULL;
  }
  else {
    AVAudioFifo *fifo = stream->audio_fifo;
    AVFrame * output_frame = stream->enc_frame;
  
    int fifo_size = av_audio_fifo_size(fifo);
    int frame_size = fifo_size;

    if(frame != NULL) {
      frame_size = enc_ctx->frame_size;
      fifo_size += frame->nb_samples;
    
      ret = av_audio_fifo_realloc(fifo, fifo_size);
      if (ret < 0) Fail("Failed to reallocate audio FIFO");

      // Store the new samples in the FIFO buffer.
      ret = av_audio_fifo_write(fifo, (void **)(const uint8_t**)frame->extended_data, frame->nb_samples);
      if (ret < frame->nb_samples) Fail("Failed to write data to audio FIFO");
    }

    for(; fifo_size >= frame_size || frame == NULL; fifo_size = av_audio_fifo_size(fifo)) {

      if(fifo_size > 0) {
        ret = av_frame_make_writable(output_frame);
        if (ret < 0) Fail("Failed to make output frame writable : %s", av_err2str(ret));

        int read_size = av_audio_fifo_read(fifo, (void **)output_frame->data, frame_size);
        if (read_size < frame_size) Fail("Failed to read data from audio FIFO");

        output_frame->nb_samples = frame_size;
        output_frame->pts = stream->pts;
        stream->pts += frame_size;
      }
      else {
        output_frame = NULL;
      }

      ret = write_frame(av, stream_index, enc_ctx, output_frame);
    
      if (ret == AVERROR_EOF) return stream;
      if (ret < 0 && ret != AVERROR(EAGAIN)) return NULL;
    }
  }
  return stream;
}

static AVFrame * scale_video_frame(stream_t * stream, AVFrame * frame)
{
  AVCodecContext * enc_ctx = stream->codec_context;

  if( ! stream->sws_ctx) {
    // create scale context
    stream->sws_ctx = sws_getContext(frame->width, frame->height,
                                     (enum AVPixelFormat)frame->format,
                                     enc_ctx->width, enc_ctx->height,
                                     enc_ctx->pix_fmt,
                                     SWS_BICUBIC, NULL, NULL, NULL);
    if ( ! stream->sws_ctx) Fail("Failed to allocate scale context");

    // Allocate the scale frame
    stream->sw_frame = av_frame_alloc();
    if( ! stream->sw_frame) Fail("Failed to allocate scale frame");

    stream->sw_frame->width = enc_ctx->width;
    stream->sw_frame->height = enc_ctx->height;
    stream->sw_frame->format = enc_ctx->pix_fmt;

    int ret = av_frame_get_buffer(stream->sw_frame, 32);
    if (ret < 0) Fail("Failed to allocate scale frame data : %s)", av_err2str(ret));
  }
  
  int ret = av_frame_make_writable(stream->sw_frame);
  if (ret < 0) Fail("Failed to make scale frame writable : %s", av_err2str(ret));

  // convert to destination format
  sws_scale(stream->sws_ctx,
            (const uint8_t * const *)frame->data, frame->linesize,
            0, frame->height,
            stream->sw_frame->data, stream->sw_frame->linesize);

  return stream->sw_frame;
}

static stream_t * write_video_frame(av_t * av, int stream_index, AVFrame * frame)
{
  if(av->format_context->nb_streams == 0) {
    if( ! av->format_context->oformat) Fail("Failed to create undefined output format");

    enum AVCodecID codec_id = av->format_context->oformat->video_codec;
    AVCodec * codec = avcodec_find_encoder(codec_id);
    enum AVPixelFormat pix_format = (enum AVPixelFormat)frame->format;

    if(codec && codec->pix_fmts) {
      pix_format = codec->pix_fmts[0];
    }
      
    stream_t * stream = new_video_stream(av, av->format_context->oformat->video_codec,
                                         frame->width, frame->height, pix_format,
                                         frame->width * frame->height * 4, 25,
                                         (AVRational){1, 25});
    if( ! stream) return NULL;
  }

  if( ! av->streams) Fail("Failed to write in closed output");

  stream_t * stream = av->streams[stream_index];
  
  if( ! stream->codec_context) Fail("Failed to write video frame with no encoder");

  AVCodecContext * enc_ctx = stream->codec_context;
  
  if(frame && (frame->width != enc_ctx->width
               || frame->height != enc_ctx->height
               || ((enum AVPixelFormat)frame->format) != enc_ctx->pix_fmt)) {

    frame = scale_video_frame(stream, frame);
    if( ! frame) return NULL;
  }

  if(frame) {
    frame->pts = stream->pts++;
  }
    
  int ret = write_frame(av, stream_index, enc_ctx, frame);
  if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) return NULL;

  return stream;
}

static stream_t * write_subtitle_frame(av_t * av, int stream_index, AVSubtitle * subtitle)
{
  stream_t * stream = av->streams[stream_index];
  AVStream * avstream = av->format_context->streams[stream->index];
  AVCodecContext * enc_ctx = stream->codec_context;
  
  if( ! stream->codec_context) Fail("Failed to write subtitle frame with no encoder");

  int ret, size = 512;
  AVPacket packet;
  av_init_packet(&packet);
  packet.data = NULL;
  packet.size = 0;
  
  ret = av_new_packet(&packet, size);
  if (ret < 0) Fail("Failed to allocate subtitle packet : %s", av_err2str(ret));

  ret = avcodec_encode_subtitle(stream->codec_context, packet.data, packet.size, subtitle);
  if (ret < 0) Fail("Failed to encode subtitle : %s", av_err2str(ret));

  packet.pts = subtitle->pts;
  packet.duration = subtitle->end_display_time - subtitle->pts;
  packet.dts = subtitle->pts;
  av_packet_rescale_ts(&packet, enc_ctx->time_base, avstream->time_base);

  packet.stream_index = stream_index;
  packet.pos = -1;

  ret = av_interleaved_write_frame(av->format_context, &packet);

  av_packet_unref(&packet);

  if(ret < 0) Fail("Failed to write subtitle packet : %s", av_err2str(ret));

  return stream;
}

CAMLprim value ocaml_av_write_stream_frame(value _stream, value _frame)
{
  CAMLparam2(_stream, _frame);
  av_t * av = StreamAv_val(_stream);
  int index = StreamIndex_val(_stream);
  stream_t * stream = NULL;
  
  if( ! av->streams) Raise(EXN_FAILURE, "Failed to write in closed output");

  enum AVMediaType type = av->streams[index]->codec_context->codec_type;

  caml_release_runtime_system();
  if(type == AVMEDIA_TYPE_AUDIO) {
    stream = write_audio_frame(av, index, Frame_val(_frame));
  }
  else if(type == AVMEDIA_TYPE_VIDEO) {
    stream = write_video_frame(av, index, Frame_val(_frame));
  }
  else if(type == AVMEDIA_TYPE_SUBTITLE) {
    stream = write_subtitle_frame(av, index, Subtitle_val(_frame));
  }
  caml_acquire_runtime_system();

  if( ! stream) Raise(EXN_FAILURE, "%s", ocaml_av_error_msg);
  CAMLreturn(Val_unit);
}

CAMLprim value ocaml_av_write_audio_frame(value _av, value _frame) {
  CAMLparam2(_av, _frame);
  av_t * av = Av_val(_av);
  AVFrame * frame = Frame_val(_frame);

  caml_release_runtime_system();
  stream_t * stream = write_audio_frame(av, 0, frame);
  caml_acquire_runtime_system();
  if( ! stream) Raise(EXN_FAILURE, "%s", ocaml_av_error_msg);

  CAMLreturn(Val_unit);
}

CAMLprim value ocaml_av_write_video_frame(value _av, value _frame) {
  CAMLparam2(_av, _frame);
  av_t * av = Av_val(_av);
  AVFrame * frame = Frame_val(_frame);

  caml_release_runtime_system();
  stream_t * stream = write_video_frame(av, 0, frame);
  caml_acquire_runtime_system();
  if( ! stream) Raise(EXN_FAILURE, "%s", ocaml_av_error_msg);

  CAMLreturn(Val_unit);
}


CAMLprim value ocaml_av_close(value _av)
{
  CAMLparam1(_av);
  av_t * av = Av_val(_av);
  stream_t no_stream;
  stream_t * stream = &no_stream;
  
  caml_release_runtime_system();

  if( ! av->is_input && av->streams) {
    // flush encoders of the output file
    unsigned int i;
    for(i = 0; i < av->format_context->nb_streams && stream; i++) {

      AVCodecContext * enc_ctx = av->streams[i]->codec_context;

      if( ! enc_ctx) continue;
  
      if(enc_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
        stream = write_audio_frame(av, i, NULL);
      }
      else if(enc_ctx->codec_type == AVMEDIA_TYPE_VIDEO) {
        stream = write_video_frame(av, i, NULL);
      }
    }

    // write the trailer
    av_write_trailer(av->format_context);
  }
  close_av(av);

  caml_acquire_runtime_system();

  if( ! stream) Raise(EXN_FAILURE, "%s", ocaml_av_error_msg);

  CAMLreturn(Val_unit);
}
