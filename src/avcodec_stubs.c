#include <caml/mlvalues.h>
#include <caml/memory.h>
#include <caml/alloc.h>
#include <caml/fail.h>
#include <caml/callback.h>
#include <caml/custom.h>
#include <caml/bigarray.h>
#include <caml/threads.h>

#include <libavformat/avformat.h>
#include "avutil_stubs.h"

#include "avcodec_stubs.h"
#include "codec_id_stubs.h"


/***** AVCodecParameters *****/

static void finalize_codec_parameters(value v)
{
  struct AVCodecParameters *codec_parameters = CodecParameters_val(v);
  avcodec_parameters_free(&codec_parameters);
}

static struct custom_operations codec_parameters_ops =
  {
    "ocaml_avcodec_parameters",
    finalize_codec_parameters,
    custom_compare_default,
    custom_hash_default,
    custom_serialize_default,
    custom_deserialize_default
  };

void value_of_codec_parameters_copy(AVCodecParameters *src, value * pvalue)
{
  if( ! src) Raise(EXN_FAILURE, "Failed to get codec parameters");

  AVCodecParameters * dst = avcodec_parameters_alloc();
  if( ! dst) Raise(EXN_FAILURE, "Failed to alloc codec parameters");

  int ret = avcodec_parameters_copy(dst, src);
  if(ret < 0) Raise(EXN_FAILURE, "Failed to copy codec parameters : %s", av_err2str(ret));

  *pvalue = caml_alloc_custom(&codec_parameters_ops, sizeof(AVCodecParameters*), 0, 1);
  CodecParameters_val(*pvalue) = dst;
}

  
/***** AVCodecContext *****/

typedef struct {
  AVCodecContext *codec_context;
  AVCodecParserContext *parser;

  // input
  uint8_t *buf;
  size_t   buf_size;
  uint8_t *remaining_data;
  size_t   remaining_data_size;
  // output
  AVFrame ** frames;
  int frames_capacity;
} codec_context_t;

#define CodecContext_val(v) (*(codec_context_t**)Data_custom_val(v))

static void finalize_codec_context(value v)
{
  codec_context_t *ctx = CodecContext_val(v);

  if(ctx->codec_context) avcodec_free_context(&ctx->codec_context);

  if(ctx->parser) av_parser_close(ctx->parser);

  if(ctx->buf) free(ctx->buf);

  if(ctx->frames) free(ctx->frames);

  free(ctx);
}

static struct custom_operations codec_context_ops =
  {
    "ocaml_codec_context",
    finalize_codec_context,
    custom_compare_default,
    custom_hash_default,
    custom_serialize_default,
    custom_deserialize_default
  };

static int decode(codec_context_t *ctx, uint8_t *data, size_t data_size)
{
  AVPacket packet;
  uint8_t *buf = ctx->buf;
  size_t needed_size = ctx->remaining_data_size + data_size + AV_INPUT_BUFFER_PADDING_SIZE;

  if(needed_size > ctx->buf_size) {

    buf = (uint8_t *)malloc(needed_size);
    if ( ! buf) return AVERROR_INVALIDDATA;

    memset(buf + ctx->remaining_data_size + data_size, 0, AV_INPUT_BUFFER_PADDING_SIZE);

    if(ctx->remaining_data_size > 0) {
      memcpy(buf, ctx->remaining_data, ctx->remaining_data_size);
    }

    if(ctx->buf) {
      free(ctx->buf);
    }

    ctx->buf = buf;
    ctx->buf_size = needed_size;
  }
  else if(ctx->remaining_data_size > 0) {
    memmove(buf, ctx->remaining_data, ctx->remaining_data_size);
  }

  memcpy(buf + ctx->remaining_data_size, data, data_size);

  ctx->remaining_data = buf;
  ctx->remaining_data_size += data_size;
  int ret = 0, nb_frames = 0;
  
  av_init_packet(&packet);
  packet.data = NULL;
  packet.size = 0;

  while((ret >= 0 || ret == AVERROR(EAGAIN))
        && (ctx->remaining_data_size > 0 || data_size == 0)) {

    if(data_size > 0) {
      ret = av_parser_parse2(ctx->parser, ctx->codec_context, &packet.data, &packet.size,
                             ctx->remaining_data, ctx->remaining_data_size,
                             AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
      if (ret < 0) break;

      ctx->remaining_data += ret;
      ctx->remaining_data_size -= ret;

      if ( ! packet.size) break;
    }

    // send the packet with the compressed data to the decoder
    ret = avcodec_send_packet(ctx->codec_context, &packet);
    if (ret < 0 && ret != AVERROR(EAGAIN)) break;

    // read all the output frames (in general there may be any number of them
    do {
      if(nb_frames >= ctx->frames_capacity) {
        ctx->frames_capacity += 1024;
        ctx->frames = (AVFrame**)realloc(ctx->frames, ctx->frames_capacity * sizeof(AVFrame*));
        if (!ctx->frames) {
          ret = AVERROR_INVALIDDATA;
          break;
        }
      }
    
      AVFrame * frame = av_frame_alloc();
      if (!frame) {
        ret = AVERROR_INVALIDDATA;
        break;
      }
    
      ret = avcodec_receive_frame(ctx->codec_context, frame);
      if (ret < 0) {
        av_frame_free(&frame);
        break;
      }
      ctx->frames[nb_frames++] = frame;
    } while (1);
  }

  if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
    while(nb_frames > 0) av_frame_free(&ctx->frames[--nb_frames]);
    return ret;
  }

  return nb_frames;
}

CAMLprim value ocaml_avcodec_decode(value _ctx, value _data, value _size)
{
  CAMLparam2(_ctx, _data);
  CAMLlocal2(val_frame, ans);
  codec_context_t *ctx = CodecContext_val(_ctx);
  uint8_t *data = Caml_ba_data_val(_data);
  int i, ret, size = Int_val(_size);

  caml_release_runtime_system();
  ret = decode(ctx, data, size);
  caml_acquire_runtime_system();

  if(ret < 0) Raise(EXN_FAILURE, "Failed to decode data : %s", av_err2str(ret));

  ans = caml_alloc_tuple(ret);

  for(i = 0; i < ret; i++) {
    value_of_frame(ctx->frames[i], &val_frame);
    Store_field(ans, i, val_frame);
  }

  CAMLreturn(ans);
}


/**** codec ****/

static enum AVCodecID find_codec_id(const char *name)
{
  av_register_all();

  AVCodec * codec = avcodec_find_encoder_by_name(name);
  if( ! codec) Raise(EXN_FAILURE, "Failed to find %s codec", name);

  return codec->id;
}

CAMLprim value ocaml_avcodec_parameters_get_bit_rate(value _cp) {
  CAMLparam1(_cp);
  CAMLreturn(Val_int(CodecParameters_val(_cp)->bit_rate));
}

/**** Audio codec ID ****/

CAMLprim value ocaml_avcodec_get_audio_codec_name(value _codec_id)
{
  CAMLparam1(_codec_id);
  CAMLreturn(caml_copy_string(avcodec_get_name((enum AVCodecID)AudioCodecID_val(_codec_id))));
}

CAMLprim value ocaml_avcodec_find_audio_codec_id(value _name)
{
  CAMLparam1(_name);
  CAMLreturn(Val_AudioCodecID(find_codec_id(String_val(_name))));
}


CAMLprim value ocaml_avcodec_find_best_sample_format(value _audio_codec_id)
{
  CAMLparam1(_audio_codec_id);
  enum AVCodecID codec_id = AudioCodecID_val(_audio_codec_id);

  av_register_all();

  AVCodec * codec = avcodec_find_encoder(codec_id);
  if( ! codec) Raise(EXN_FAILURE, "Failed to find codec");
  if( ! codec->sample_fmts) Raise(EXN_FAILURE, "Failed to find codec sample formats");
    
  CAMLreturn(Val_SampleFormat(codec->sample_fmts[0]));
}

/**** Audio codec parameters ****/
CAMLprim value ocaml_avcodec_parameters_get_audio_codec_id(value _cp) {
  CAMLparam1(_cp);
  CAMLreturn(Val_AudioCodecID(CodecParameters_val(_cp)->codec_id));
}

CAMLprim value ocaml_avcodec_parameters_get_channel_layout(value _cp) {
  CAMLparam1(_cp);
  AVCodecParameters * cp = CodecParameters_val(_cp);

  if(cp->channel_layout == 0) {
    cp->channel_layout = av_get_default_channel_layout(cp->channels);
  }

  CAMLreturn(Val_ChannelLayout(cp->channel_layout));
}

CAMLprim value ocaml_avcodec_parameters_get_nb_channels(value _cp) {
  CAMLparam1(_cp);
  CAMLreturn(Val_int(CodecParameters_val(_cp)->channels));
}

CAMLprim value ocaml_avcodec_parameters_get_sample_format(value _cp) {
  CAMLparam1(_cp);
  CAMLreturn(Val_SampleFormat((enum AVSampleFormat)CodecParameters_val(_cp)->format));
}

CAMLprim value ocaml_avcodec_parameters_get_sample_rate(value _cp) {
  CAMLparam1(_cp);
  CAMLreturn(Val_int(CodecParameters_val(_cp)->sample_rate));
}

CAMLprim value ocaml_avcodec_parameters_audio_copy(value _codec_id, value _channel_layout, value _sample_format, value _sample_rate, value _cp) {
  CAMLparam4(_codec_id, _channel_layout, _sample_format, _cp);
  CAMLlocal1(ans);

  value_of_codec_parameters_copy(CodecParameters_val(_cp), &ans);
  
  AVCodecParameters * dst = CodecParameters_val(ans);

  dst->codec_id = AudioCodecID_val(_codec_id);
  dst->channel_layout = ChannelLayout_val(_channel_layout);
  dst->channels = av_get_channel_layout_nb_channels(dst->channel_layout);
  dst->format = SampleFormat_val(_sample_format);
  dst->sample_rate = Int_val(_sample_rate);

  CAMLreturn(ans);
}


static AVCodecContext * avcodec_audio_create_context(enum AVCodecID codec_id, uint64_t channel_layout, enum AVSampleFormat sample_fmt, int bit_rate, int sample_rate)
{
  av_register_all();

  AVCodec *codec = avcodec_find_decoder(codec_id);
  if( ! codec) Fail("Failed to create audio codec");
  
  AVCodecContext *codec_context = avcodec_alloc_context3(codec);
  if( ! codec_context) Fail("Failed to allocate audio codec context");

  codec_context->channel_layout = channel_layout;
  codec_context->channels = av_get_channel_layout_nb_channels(channel_layout);
  codec_context->sample_fmt = sample_fmt;
  codec_context->sample_rate = sample_rate;
  codec_context->time_base = (AVRational){1, codec_context->sample_rate};

  if(bit_rate > 0) codec_context->bit_rate = bit_rate;

  // Open the decoder
  int ret = avcodec_open2(codec_context, codec, NULL);
  if(ret < 0) {
    avcodec_free_context(&codec_context);
    Fail("Failed to open codec : %s", av_err2str(ret));
  }
  return codec_context;
}

CAMLprim value ocaml_avcodec_audio_create_context(value _codec_id, value _channel_layout, value _sample_format, value _bit_rate, value _sample_rate) {
  CAMLparam4(_codec_id, _channel_layout, _sample_format, _bit_rate);
  CAMLlocal1(ans);
  int bit_rate = _bit_rate == Int_val(0) ? -1 : Int_val(Field(_bit_rate, 0));

  codec_context_t * ctx = (codec_context_t*)calloc(1, sizeof(codec_context_t));
  if ( ! ctx) Raise(EXN_FAILURE, "Failed to allocate codec context");

  ans = caml_alloc_custom(&codec_context_ops, sizeof(codec_context_t*), 0, 1);
  CodecContext_val(ans) = ctx;
  
  caml_release_runtime_system();
  ctx->codec_context = avcodec_audio_create_context(AudioCodecID_val(_codec_id),
                                                    ChannelLayout_val(_channel_layout),
                                                    SampleFormat_val(_sample_format),
                                                    bit_rate,
                                                    Int_val(_sample_rate));
  caml_acquire_runtime_system();
  if( ! ctx->codec_context) Raise(EXN_FAILURE, ocaml_av_error_msg);

  ctx->parser = av_parser_init(ctx->codec_context->codec->id);
  if ( ! ctx->parser) Raise(EXN_FAILURE, "Failed to init codec parser");

  CAMLreturn(ans);
}


/**** Video codec ID ****/

CAMLprim value ocaml_avcodec_get_video_codec_name(value _codec_id)
{
  CAMLparam1(_codec_id);
  CAMLreturn(caml_copy_string(avcodec_get_name(VideoCodecID_val(_codec_id))));
}

CAMLprim value ocaml_avcodec_find_video_codec_id(value _name)
{
  CAMLparam1(_name);
  CAMLreturn(Val_VideoCodecID(find_codec_id(String_val(_name))));
}

CAMLprim value ocaml_avcodec_find_best_pixel_format(value _video_codec_id)
{
  CAMLparam1(_video_codec_id);
  enum AVCodecID codec_id = VideoCodecID_val(_video_codec_id);

  av_register_all();

  AVCodec * codec = avcodec_find_encoder(codec_id);
  if( ! codec) Raise(EXN_FAILURE, "Failed to find codec");
  if( ! codec->pix_fmts) Raise(EXN_FAILURE, "Failed to find codec pixel formats");
    
  CAMLreturn(Val_PixelFormat(codec->pix_fmts[0]));
}


/**** Video codec parameters ****/
CAMLprim value ocaml_avcodec_parameters_get_video_codec_id(value _cp) {
  CAMLparam1(_cp);
  CAMLreturn(Val_VideoCodecID(CodecParameters_val(_cp)->codec_id));
}

CAMLprim value ocaml_avcodec_parameters_get_width(value _cp) {
  CAMLparam1(_cp);
  CAMLreturn(Val_int(CodecParameters_val(_cp)->width));
}

CAMLprim value ocaml_avcodec_parameters_get_height(value _cp) {
  CAMLparam1(_cp);
  CAMLreturn(Val_int(CodecParameters_val(_cp)->height));
}

CAMLprim value ocaml_avcodec_parameters_get_sample_aspect_ratio(value _cp) {
  CAMLparam1(_cp);
  CAMLlocal1(ans);

  ans = caml_alloc_tuple(2);
  Field(ans, 0) = Val_int(CodecParameters_val(_cp)->sample_aspect_ratio.num);
  Field(ans, 1) = Val_int(CodecParameters_val(_cp)->sample_aspect_ratio.den);

  CAMLreturn(ans);
}

CAMLprim value ocaml_avcodec_parameters_get_pixel_format(value _cp) {
  CAMLparam1(_cp);
  CAMLreturn(Val_PixelFormat((enum AVPixelFormat)CodecParameters_val(_cp)->format));
}

CAMLprim value ocaml_avcodec_parameters_video_copy(value _codec_id, value _width, value _height, value _sample_aspect_ratio, value _pixel_format, value _bit_rate, value _cp) {
  CAMLparam4(_codec_id, _sample_aspect_ratio, _pixel_format, _cp);
  CAMLlocal1(ans);

  value_of_codec_parameters_copy(CodecParameters_val(_cp), &ans);
  
  AVCodecParameters * dst = CodecParameters_val(ans);

  dst->codec_id = VideoCodecID_val(_codec_id);
  dst->width = Int_val(_width);
  dst->height = Int_val(_height);
  dst->sample_aspect_ratio.num = Int_val(Field(_sample_aspect_ratio, 0));
  dst->sample_aspect_ratio.den = Int_val(Field(_sample_aspect_ratio, 1));
  dst->format = PixelFormat_val(_pixel_format);
  dst->bit_rate = Int_val(_bit_rate);

  CAMLreturn(ans);
}

CAMLprim value ocaml_avcodec_parameters_video_copy_byte(value *argv, int argn)
{
  return ocaml_avcodec_parameters_video_copy(argv[0], argv[1], argv[2], argv[3], argv[4], argv[5], argv[7]);
}


/**** Subtitle codec ID ****/

CAMLprim value ocaml_avcodec_get_subtitle_codec_name(value _codec_id)
{
  CAMLparam1(_codec_id);
  CAMLreturn(caml_copy_string(avcodec_get_name(SubtitleCodecID_val(_codec_id))));
}

CAMLprim value ocaml_avcodec_find_subtitle_codec_id(value _name)
{
  CAMLparam1(_name);
  CAMLreturn(Val_SubtitleCodecID(find_codec_id(String_val(_name))));
}

/**** Subtitle codec parameters ****/
CAMLprim value ocaml_avcodec_parameters_get_subtitle_codec_id(value _cp) {
  CAMLparam1(_cp);
  CAMLreturn(Val_SubtitleCodecID(CodecParameters_val(_cp)->codec_id));
}

CAMLprim value ocaml_avcodec_parameters_subtitle_copy(value _codec_id, value _cp) {
  CAMLparam2(_codec_id, _cp);
  CAMLlocal1(ans);

  value_of_codec_parameters_copy(CodecParameters_val(_cp), &ans);
  
  AVCodecParameters * dst = CodecParameters_val(ans);

  dst->codec_id = SubtitleCodecID_val(_codec_id);

  CAMLreturn(ans);
}
