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
  AVCodec *codec;
  AVCodecContext *codec_context;
  AVCodecParserContext *parser;
  uint8_t *buf;
  size_t   buf_size;
  // input
  uint8_t *remaining_data;
  size_t   remaining_data_size;
  // output
  int bit_rate;
  int frame_rate;
  AVFrame ** frames;
  int frames_capacity;
  AVPacket ** packets;
  int packets_capacity;
} codec_context_t;

#define CodecContext_val(v) (*(codec_context_t**)Data_custom_val(v))

static void free_codec_context(codec_context_t *ctx)
{
  if( ! ctx) return;

  if(ctx->codec_context) avcodec_free_context(&ctx->codec_context);

  if(ctx->parser) av_parser_close(ctx->parser);

  if(ctx->buf) free(ctx->buf);

  if(ctx->frames) free(ctx->frames);

  if(ctx->packets) {
    int i;
    for(i = 0; ctx->packets[i]; i++) av_packet_free(&ctx->packets[i]);

    free(ctx->packets);
  }
  
  free(ctx);
}

static void finalize_codec_context(value v)
{
  free_codec_context(CodecContext_val(v));
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

static codec_context_t * avcodec_create_codec_context(enum AVCodecID codec_id, int decoder)
{
  av_register_all();

  codec_context_t * ctx = (codec_context_t*)calloc(1, sizeof(codec_context_t));
  if ( ! ctx) Fail("Failed to allocate codec context");

  if(decoder) {
    ctx->codec = avcodec_find_decoder(codec_id);

    if(ctx->codec) ctx->codec_context = avcodec_alloc_context3(ctx->codec);

    if( ! ctx->codec_context) {
      free_codec_context(ctx);
      Fail("Failed to allocate codec context");
    }

    // Open the codec
    int ret = avcodec_open2(ctx->codec_context, NULL, NULL);
    if(ret < 0) {
      free_codec_context(ctx);
      Fail("Failed to open codec : %s", av_err2str(ret));
    }

    ctx->parser = av_parser_init(codec_id);
    if ( ! ctx->parser) {
      free_codec_context(ctx);
      Fail("Failed to init codec parser");
    }
  }
  else {
    ctx->codec = avcodec_find_encoder(codec_id);

    if( ! ctx->codec) {
      free_codec_context(ctx);
      Fail("Failed to find codec");
    }
    // in case of encoding, the AVCodecContext will be created with the properties of the first sended frame
  }

  return ctx;
}

CAMLprim value ocaml_avcodec_create_context(value _codec_id, value _decoder, value _bit_rate, value _frame_rate) {
  CAMLparam3(_codec_id, _bit_rate, _frame_rate);
  CAMLlocal1(ans);

  caml_release_runtime_system();
  codec_context_t * ctx = avcodec_create_codec_context(AudioCodecID_val(_codec_id), Int_val(_decoder));
  caml_acquire_runtime_system();

  if( ! ctx) Raise(EXN_FAILURE, ocaml_av_error_msg);

  ans = caml_alloc_custom(&codec_context_ops, sizeof(codec_context_t*), 0, 1);
  CodecContext_val(ans) = ctx;

  ctx->bit_rate = Is_block(_bit_rate) ? Int_val(Field(_bit_rate, 0)) : 0;
  ctx->frame_rate = Is_block(_frame_rate) ? Int_val(Field(_frame_rate, 0)) : 0;

  CAMLreturn(ans);
}


static AVCodecContext * avcodec_open_codec(AVCodec *codec, AVFrame *frame, int bit_rate, int frame_rate)
{
  if(codec->type != AVMEDIA_TYPE_AUDIO && codec->type != AVMEDIA_TYPE_VIDEO) Fail("Failed to open unsupported codec type");

  AVCodecContext *ctx = avcodec_alloc_context3(codec);
  if( ! ctx) {
    Fail("Failed to allocate codec context");
  }

  if(codec->type == AVMEDIA_TYPE_AUDIO) {
    ctx->channel_layout = frame->channel_layout;
    ctx->channels = frame->channels;
    ctx->sample_fmt = (enum AVSampleFormat)frame->format;
    ctx->sample_rate = frame->sample_rate;
    ctx->time_base = (AVRational){1, frame->sample_rate};
  }
  else {
    ctx->width = frame->width;
    ctx->height = frame->height;
    ctx->pix_fmt = (enum AVPixelFormat)frame->format;
    ctx->framerate = (AVRational){frame_rate, 1};
    ctx->time_base = (AVRational){1, frame_rate};
  }

  ctx->bit_rate = bit_rate;

  // Open the codec
  int ret = avcodec_open2(ctx, NULL, NULL);
  if(ret < 0) {
    avcodec_free_context(&ctx);
    Fail("Failed to open codec : %s", av_err2str(ret));
  }

  printf("ctx->frame_size = %d, frame->nb_samples = %d\n", ctx->frame_size, frame->nb_samples);
  return ctx;
}

static value decode_to_frame(codec_context_t *ctx, uint8_t *data, size_t data_size)
{
  CAMLparam0();
  CAMLlocal2(val_frame, ans);
  AVPacket packet;
  uint8_t *buf = ctx->buf;
  int i, ret = 0, nb_frames = 0;
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
  
  caml_release_runtime_system();

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

  caml_acquire_runtime_system();

  if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
    while(nb_frames > 0) av_frame_free(&ctx->frames[--nb_frames]);

    Raise(EXN_FAILURE, "Failed to decode data : %s", av_err2str(ret));
  }

  ans = caml_alloc_tuple(nb_frames);

  for(i = 0; i < nb_frames; i++) {
    value_of_frame(ctx->frames[i], &val_frame);
    Store_field(ans, i, val_frame);
  }

  CAMLreturn(ans);
}

CAMLprim value ocaml_avcodec_decode(value _ctx, value _data, value _size)
{
  CAMLparam2(_ctx, _data);
  CAMLreturn(decode_to_frame(CodecContext_val(_ctx), (uint8_t*)String_val(_data), Int_val(_size)));
}

CAMLprim value ocaml_avcodec_decode_data(value _ctx, value _data, value _size)
{
  CAMLparam2(_ctx, _data);
  CAMLreturn(decode_to_frame(CodecContext_val(_ctx), Caml_ba_data_val(_data), Int_val(_size)));
}


static int encode_frame(codec_context_t *ctx, AVFrame *frame, intnat *data_size)
{
  int i, nb_packets = 0;

  if(!ctx->codec_context
     && ! (ctx->codec_context = avcodec_open_codec(ctx->codec, frame, ctx->bit_rate, ctx->frame_rate))) {
    Raise(EXN_FAILURE, ocaml_av_error_msg);
  }

  caml_release_runtime_system();

  // send the frame for encoding
  int ret = avcodec_send_frame(ctx->codec_context, frame);
  if (ret < 0) return ret;

  // read all the available output packets
  while (ret >= 0) {
    if(nb_packets >= ctx->packets_capacity) {
      AVPacket **packets = (AVPacket **)calloc(ctx->packets_capacity + 33, sizeof(AVPacket *));

      if( ! packets) return AVERROR_INVALIDDATA;

      if(ctx->packets) {
        for(i = 0; ctx->packets[i]; i++) packets[i] = ctx->packets[i];
        free(ctx->packets);
      }
      ctx->packets = packets;
      ctx->packets_capacity += 32;
    }

    if( ! ctx->packets[nb_packets]) {
      ctx->packets[nb_packets] = av_packet_alloc();

      if( ! ctx->packets[nb_packets]) return AVERROR_INVALIDDATA;
    }
    ret = avcodec_receive_packet(ctx->codec_context, ctx->packets[nb_packets]);
    if (ret < 0) break;

    *data_size += ctx->packets[nb_packets]->size;
    nb_packets++;
  }

  caml_acquire_runtime_system();

  if(ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) Raise(EXN_FAILURE, "Failed to encode data : %s", av_err2str(ret));

  return nb_packets;
}

CAMLprim value ocaml_avcodec_encode(value _ctx, value _frame)
{
  CAMLparam2(_ctx, _frame);
  CAMLlocal1(ans);
  codec_context_t *ctx = CodecContext_val(_ctx);
  intnat data_size = 0;
  int i, ret = 0;

  ret = encode_frame(ctx, Frame_val(_frame), &data_size);

  ans = caml_alloc_string(data_size);
  
  uint8_t * data = (uint8_t*)String_val(ans);

  for(i = 0; i < ret; i++) {
    memcpy(data, ctx->packets[i]->data, ctx->packets[i]->size);
    data += ctx->packets[i]->size;
    av_packet_unref(ctx->packets[i]);
  }
  
  CAMLreturn(ans);
}

CAMLprim value ocaml_avcodec_encode_to_data(value _ctx, value _frame)
{
  CAMLparam2(_ctx, _frame);
  CAMLlocal1(ans);
  codec_context_t *ctx = CodecContext_val(_ctx);
  intnat data_size = 0;
  int i, ret = 0;

  ret = encode_frame(ctx, Frame_val(_frame), &data_size);

  ans = caml_ba_alloc(CAML_BA_C_LAYOUT | CAML_BA_UINT8, 1, NULL, &data_size);
  
  uint8_t * data = Caml_ba_data_val(ans);

  for(i = 0; i < ret; i++) {
    memcpy(data, ctx->packets[i]->data, ctx->packets[i]->size);
    data += ctx->packets[i]->size;
    av_packet_unref(ctx->packets[i]);
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
