#include <caml/mlvalues.h>
#include <caml/memory.h>
#include <caml/alloc.h>
#include <caml/fail.h>
#include <caml/callback.h>
#include <caml/custom.h>
#include <caml/bigarray.h>
#include <caml/threads.h>

#include <libavformat/avformat.h>
#include <libavutil/audio_fifo.h>
#include "avutil_stubs.h"

#include "avcodec_stubs.h"
#include "codec_id_stubs.h"


CAMLprim value ocaml_avcodec_get_input_buffer_padding_size() {
  CAMLparam0();
  CAMLreturn(Val_int(AV_INPUT_BUFFER_PADDING_SIZE));
}

CAMLprim value ocaml_avcodec_audio_codec_id_to_AVCodecID(value _codec_id) {
  CAMLparam1(_codec_id);
  CAMLreturn(Val_int(AudioCodecID_val(_codec_id)));
}

CAMLprim value ocaml_avcodec_video_codec_id_to_AVCodecID(value _codec_id) {
  CAMLparam1(_codec_id);
  CAMLreturn(Val_int(VideoCodecID_val(_codec_id)));
}

CAMLprim value ocaml_avcodec_subtitle_codec_id_to_AVCodecID(value _codec_id) {
  CAMLparam1(_codec_id);
  CAMLreturn(Val_int(SubtitleCodecID_val(_codec_id)));
}


/***** AVCodecContext *****/

static AVCodecContext * avcodec_create_AVCodecContext(AVCodec *codec)
{
  AVCodecContext *codec_context = NULL;

  if(codec) codec_context = avcodec_alloc_context3(codec);

  if( ! codec_context) {
    Fail("Failed to allocate codec context");
  }

  // Open the codec
  int ret = avcodec_open2(codec_context, codec, NULL);
  if(ret < 0) {
    avcodec_free_context(&codec_context);
    Fail("Failed to open codec : %s", av_err2str(ret));
  }

  return codec_context;
}


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

  
/***** AVPacket *****/

static void finalize_packet(value v)
{
  struct AVPacket *packet = Packet_val(v);
  av_packet_free(&packet);
}

static struct custom_operations packet_ops =
  {
    "ocaml_packet",
    finalize_packet,
    custom_compare_default,
    custom_hash_default,
    custom_serialize_default,
    custom_deserialize_default
  };

void value_of_packet(AVPacket *packet, value * pvalue)
{
  if( ! packet) Raise(EXN_FAILURE, "Empty packet");

  *pvalue = caml_alloc_custom(&packet_ops, sizeof(AVPacket*), 0, 1);
  Packet_val((*pvalue)) = packet;
}

CAMLprim value ocaml_avcodec_get_packet_size(value _packet)
{
  CAMLparam1(_packet);
  CAMLreturn(Val_int(Packet_val(_packet)->size));
}

CAMLprim value ocaml_avcodec_get_packet_stream_index(value _packet)
{
  CAMLparam1(_packet);
  CAMLreturn(Val_int(Packet_val(_packet)->stream_index));
}

CAMLprim value ocaml_avcodec_set_packet_stream_index(value _packet, value _index)
{
  CAMLparam1(_packet);
  Packet_val(_packet)->stream_index = Int_val(_index);
  CAMLreturn(Val_unit);
}

CAMLprim value ocaml_avcodec_packet_to_bytes(value _packet)
{
  CAMLparam1(_packet);
  CAMLlocal1(ans);
  struct AVPacket *packet = Packet_val(_packet);

  ans = caml_alloc_string(packet->size);

  memcpy((uint8_t*)String_val(ans), packet->data, packet->size);
  
  CAMLreturn(ans);
}


/***** AVCodecParserContext *****/

typedef struct {
  AVCodecParserContext *context;
  AVCodecContext *codec_context;
  AVPacket ** packets;
  //  int nb_packets;
  int packets_capacity;
} parser_t;

#define Parser_val(v) (*(parser_t**)Data_custom_val(v))

static void free_parser(parser_t *parser)
{
  if( ! parser) return;

  if(parser->context) av_parser_close(parser->context);

  if(parser->codec_context) avcodec_free_context(&parser->codec_context);

  if(parser->packets) free(parser->packets);

  free(parser);
}

static void finalize_parser(value v)
{
  free_parser(Parser_val(v));
}

static struct custom_operations parser_ops =
  {
    "ocaml_avcodec_parser",
    finalize_parser,
    custom_compare_default,
    custom_hash_default,
    custom_serialize_default,
    custom_deserialize_default
  };

static parser_t * avcodec_create_parser(enum AVCodecID codec_id)
{
  avcodec_register_all();

  AVCodec * codec = avcodec_find_decoder(codec_id);
  if ( ! codec) Fail("Failed to find parser's decoder");

  parser_t * parser = (parser_t*)calloc(1, sizeof(parser_t));
  if ( ! parser) Fail("Failed to allocate parser");

  parser->context = av_parser_init(codec->id);
  if ( ! parser->context) {
    free_parser(parser);
    Fail("Failed to init parser context");
  }
  
  parser->codec_context = avcodec_create_AVCodecContext(codec);

  if( ! parser->codec_context) {
    free_parser(parser);
    return NULL;
  }

  return parser;
}

CAMLprim value ocaml_avcodec_create_parser(value _codec_id) {
  CAMLparam1(_codec_id);
  CAMLlocal1(ans);

  caml_release_runtime_system();
  parser_t * parser = avcodec_create_parser((enum AVCodecID)Int_val(_codec_id));
  caml_acquire_runtime_system();

  if( ! parser) Raise(EXN_FAILURE, ocaml_av_error_msg);

  ans = caml_alloc_custom(&parser_ops, sizeof(parser_t*), 0, 1);
  Parser_val(ans) = parser;

  CAMLreturn(ans);
}

CAMLprim value ocaml_avcodec_parse_to_packets(value _parser, value _data, value _len)
{
  CAMLparam2(_parser, _data);
  CAMLlocal3(val_packet, packets, ans);
  parser_t *parser = Parser_val(_parser);
  uint8_t *remaining_data = Caml_ba_data_val(_data);
  size_t len = Int_val(_len);
  size_t remaining_len = len;
  int i, ret = 0, nb_packets = 0;
  
  caml_release_runtime_system();

  while(remaining_len > 0) {

    if(nb_packets >= parser->packets_capacity) {
      parser->packets_capacity += 2048;
      parser->packets = (AVPacket **)realloc(parser->packets, parser->packets_capacity * sizeof(AVPacket *));

      if( ! parser->packets) {
        ret = AVERROR_INVALIDDATA;
        break;
      }
    }

    AVPacket *packet = av_packet_alloc();
    if( ! packet) {
      ret = AVERROR_INVALIDDATA;
      break;
    }
    parser->packets[nb_packets++] = packet;

    av_init_packet(packet);
    packet->data = NULL;
    packet->size = 0;

    ret = av_parser_parse2(parser->context, parser->codec_context, &packet->data, &packet->size,
                           remaining_data, remaining_len,
                           AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
    if (ret < 0) break;

    remaining_data += ret;
    remaining_len -= ret;

    if ( ! packet->size) break;
  }

  caml_acquire_runtime_system();

  if (ret < 0) {
    while(nb_packets > 0) av_packet_free(&parser->packets[--nb_packets]);
    Raise(EXN_FAILURE, "Failed to parse data : %s", av_err2str(ret));
  }

  packets = caml_alloc_tuple(nb_packets);

  for(i = 0; i < nb_packets; i++) {
    value_of_packet(parser->packets[i], &val_packet);
    Store_field(packets, i, val_packet);
  }

  ans = caml_alloc_tuple(2);
  Store_field(ans, 0, packets);
  Store_field(ans, 1, Val_int(len - remaining_len));

  CAMLreturn(ans);
}


/***** codec_context_t *****/

typedef struct {
  AVCodec *codec;
  AVCodecContext *codec_context;
  // output
  int bit_rate;
  int frame_rate;
  AVFrame ** frames;
  int frames_capacity;
  AVPacket ** packets;
  //  int nb_packets;
  int packets_capacity;
  AVAudioFifo *audio_fifo;
  AVFrame *enc_frame;
  int64_t pts;
} codec_context_t;

#define CodecContext_val(v) (*(codec_context_t**)Data_custom_val(v))

static void free_codec_context(codec_context_t *ctx)
{
  if( ! ctx) return;

  if(ctx->codec_context) avcodec_free_context(&ctx->codec_context);

  if(ctx->frames) free(ctx->frames);

  if(ctx->packets) {
    int i;
    for(i = 0; i < ctx->packets_capacity; i++)
      if(ctx->packets[i]) av_packet_free(&ctx->packets[i]);

    free(ctx->packets);
  }
  
  if(ctx->audio_fifo) {
    av_audio_fifo_free(ctx->audio_fifo);
  }

  if(ctx->enc_frame) {
    av_frame_free(&ctx->enc_frame);
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
  avcodec_register_all();

  codec_context_t * ctx = (codec_context_t*)calloc(1, sizeof(codec_context_t));
  if ( ! ctx) Fail("Failed to allocate codec context");

  if(decoder) {
    ctx->codec = avcodec_find_decoder(codec_id);

    ctx->codec_context = avcodec_create_AVCodecContext(ctx->codec);

    if( ! ctx->codec_context) {
      free_codec_context(ctx);
      return NULL;
    }
  }
  else {
    ctx->codec = avcodec_find_encoder(codec_id);

    if( ! ctx->codec) {
      free_codec_context(ctx);
      Fail("Failed to find codec");
    }

    ctx->packets = (AVPacket **)calloc(256, sizeof(AVPacket *));
    if(ctx->packets) ctx->packets_capacity = 256;
    // in case of encoding, the AVCodecContext will be created with the properties of the first sended frame
  }

  return ctx;
}

CAMLprim value ocaml_avcodec_create_context(value _codec_id, value _decoder, value _bit_rate, value _frame_rate) {
  CAMLparam3(_codec_id, _bit_rate, _frame_rate);
  CAMLlocal1(ans);

  caml_release_runtime_system();
  codec_context_t * ctx = avcodec_create_codec_context((enum AVCodecID)Int_val(_codec_id), Int_val(_decoder));
  caml_acquire_runtime_system();

  if( ! ctx) Raise(EXN_FAILURE, ocaml_av_error_msg);

  ans = caml_alloc_custom(&codec_context_ops, sizeof(codec_context_t*), 0, 1);
  CodecContext_val(ans) = ctx;

  ctx->bit_rate = Is_block(_bit_rate) ? Int_val(Field(_bit_rate, 0)) : 0;
  ctx->frame_rate = Is_block(_frame_rate) ? Int_val(Field(_frame_rate, 0)) : 0;

  CAMLreturn(ans);
}


static AVCodecContext * avcodec_open_codec(codec_context_t *ctx, AVFrame *frame)
{
  if( ! frame) Fail("Failed to open encoder without frame");
  
  AVCodec *codec = ctx->codec;
  
  if(codec->type != AVMEDIA_TYPE_AUDIO && codec->type != AVMEDIA_TYPE_VIDEO) Fail("Failed to open unsupported encoder type");

  ctx->codec_context = avcodec_alloc_context3(codec);
  if( ! ctx->codec_context) Fail("Failed to allocate encoder context");

  if(codec->type == AVMEDIA_TYPE_AUDIO) {
    ctx->codec_context->channel_layout = frame->channel_layout;
    ctx->codec_context->channels = frame->channels;
    ctx->codec_context->sample_fmt = (enum AVSampleFormat)frame->format;
    ctx->codec_context->sample_rate = frame->sample_rate;
    ctx->codec_context->time_base = (AVRational){1, frame->sample_rate};
  }
  else {
    ctx->codec_context->width = frame->width;
    ctx->codec_context->height = frame->height;
    ctx->codec_context->pix_fmt = (enum AVPixelFormat)frame->format;
    ctx->codec_context->framerate = (AVRational){ctx->frame_rate, 1};
    ctx->codec_context->time_base = (AVRational){1, ctx->frame_rate};
  }

  ctx->codec_context->bit_rate = ctx->bit_rate;

  // Open the codec
  int ret = avcodec_open2(ctx->codec_context, NULL, NULL);
  if(ret < 0) Fail("Failed to open encoder : %s", av_err2str(ret));

  if (ctx->codec_context->frame_size > 0
      || ! (ctx->codec_context->codec->capabilities & AV_CODEC_CAP_VARIABLE_FRAME_SIZE)) {

    // Allocate the buffer frame and audio fifo if the codec doesn't support variable frame size
    ctx->enc_frame = av_frame_alloc();
    if( ! ctx->enc_frame) Fail("Failed to allocate encoder frame");

    ctx->enc_frame->nb_samples     = ctx->codec_context->frame_size;
    ctx->enc_frame->channel_layout = ctx->codec_context->channel_layout;
    ctx->enc_frame->format         = ctx->codec_context->sample_fmt;
    ctx->enc_frame->sample_rate    = ctx->codec_context->sample_rate;

    int ret = av_frame_get_buffer(ctx->enc_frame, 0);
    if (ret < 0) Fail("Failed to allocate encoder frame samples : %s)", av_err2str(ret));

    // Create the FIFO buffer based on the specified output sample format.
    ctx->audio_fifo = av_audio_fifo_alloc(ctx->codec_context->sample_fmt, ctx->codec_context->channels, 1);
    if( ! ctx->audio_fifo) Fail("Failed to allocate audio FIFO");
  }

  return ctx->codec_context;
}

static AVFrame** decode_packet_to_frames(codec_context_t *ctx, AVPacket *packet, int *p_nb_frames)
{
  int nb_frames = *p_nb_frames;
  // send the packet with the compressed data to the decoder
  int ret = avcodec_send_packet(ctx->codec_context, packet);

  // read all the output frames (in general there may be any number of them
  while (ret >= 0) {
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
  };

  if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
    while(nb_frames > 0) av_frame_free(&ctx->frames[--nb_frames]);
    Fail("Failed to decode data : %s", av_err2str(ret));
  }
  *p_nb_frames = nb_frames;
  return ctx->frames;
}

CAMLprim value ocaml_avcodec_decode(value _ctx, value _packet)
{
  CAMLparam2(_ctx, _packet);
  CAMLlocal2(val_frame, ans);
  AVPacket *packet = _packet ? Packet_val(_packet) : NULL;
  int i, nb_frames = 0;

  caml_release_runtime_system();
  AVFrame** frames = decode_packet_to_frames(CodecContext_val(_ctx), packet, &nb_frames);
  caml_acquire_runtime_system();

  if( ! frames) Raise(EXN_FAILURE, ocaml_av_error_msg);

  ans = caml_alloc_tuple(nb_frames);

  for(i = 0; i < nb_frames; i++) {
    value_of_frame(frames[i], &val_frame);
    Store_field(ans, i, val_frame);
  }

  CAMLreturn(ans);
}

CAMLprim value ocaml_avcodec_flush_decoder(value _ctx) {
  ocaml_avcodec_decode(_ctx, 0);
}


static int encode_frame_to_packets(codec_context_t *ctx, AVFrame *frame, int *nb_packets)
{
  int i, index_packet = *nb_packets;

  // send the frame for encoding
  int ret = avcodec_send_frame(ctx->codec_context, frame);
  if(ret < 0) {
    Log("Failed to encode frame : %s", av_err2str(ret));
    return ret;
  }

  // read all the available output packets
  while (ret >= 0) {
    if(index_packet >= ctx->packets_capacity) {
      AVPacket **packets = (AVPacket **)calloc(ctx->packets_capacity + 256, sizeof(AVPacket *));
      if( ! packets) {
        Log("Failed to allocate packets array");
        return AVERROR_INVALIDDATA;
      }

      if(ctx->packets) {
        for(i = 0; i < ctx->packets_capacity; i++) packets[i] = ctx->packets[i];
        free(ctx->packets);
      }
      ctx->packets = packets;
      ctx->packets_capacity += 256;
    }

    if( ! ctx->packets[index_packet]) {
      ctx->packets[index_packet] = av_packet_alloc();

      if( ! ctx->packets[index_packet]) {
        Log("Failed to allocate packet");
        return AVERROR_INVALIDDATA;
      }
    }
    ret = avcodec_receive_packet(ctx->codec_context, ctx->packets[index_packet]);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;

    if (ret < 0) {
      Log("Failed to receive packet from frame : %s", av_err2str(ret));
      break;
    }

    index_packet++;
  }

  *nb_packets = index_packet;

  return ret;
}


static AVPacket ** encode_frame(codec_context_t *ctx, AVFrame *frame, int *nb_packets)
{
  int ret;
  AVCodecContext * enc_ctx = ctx->codec_context;

  if(!ctx->codec_context && !(enc_ctx = avcodec_open_codec(ctx, frame))) {
    return NULL;
  }
  if(ctx->codec->type == AVMEDIA_TYPE_VIDEO) {
    if(frame) {
      frame->pts = ctx->pts++;
    }
    ret = encode_frame_to_packets(ctx, frame, nb_packets);
  }
  else if(ctx->audio_fifo == NULL) {

    if(frame) {
      frame->pts = ctx->pts;
      ctx->pts += frame->nb_samples;
    }
  
    ret = encode_frame_to_packets(ctx, frame, nb_packets);
  }
  else {
    AVAudioFifo *fifo = ctx->audio_fifo;
    AVFrame * output_frame = ctx->enc_frame;
  
    int fifo_size = av_audio_fifo_size(fifo);
    int frame_size = fifo_size;

    if(frame != NULL) {
      frame_size = enc_ctx->frame_size;
      fifo_size += frame->nb_samples;
    
      ret = av_audio_fifo_realloc(fifo, fifo_size);
      if (ret < 0) Fail("Failed to reallocate audio FIFO : %s", av_err2str(ret));

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
        output_frame->pts = ctx->pts;
        ctx->pts += frame_size;
      }
      else {
        output_frame = NULL;
      }

      ret = encode_frame_to_packets(ctx, output_frame, nb_packets);
      if (ret < 0 && ret != AVERROR(EAGAIN)) break;
    }
  }

  if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) return NULL;

  return ctx->packets;
}

CAMLprim value ocaml_avcodec_encode(value _ctx, value _frame)
{
  CAMLparam2(_ctx, _frame);
  CAMLlocal2(ans, val_packet);
  codec_context_t *ctx = CodecContext_val(_ctx);
  AVFrame *frame = _frame ? Frame_val(_frame) : NULL;
  int i, nb_packets = 0;

  caml_release_runtime_system();
  AVPacket ** packets = encode_frame(ctx, frame, &nb_packets);
  caml_acquire_runtime_system();

  if( ! packets) Raise(EXN_FAILURE, ocaml_av_error_msg);

  ans = caml_alloc_tuple(nb_packets);

  for(i = 0; i < nb_packets; i++) {
    value_of_packet(packets[i], &val_packet);
    Store_field(ans, i, val_packet);
    packets[i] = NULL;
  }
  
  CAMLreturn(ans);
}

CAMLprim value ocaml_avcodec_flush_encoder(value _ctx) {
  ocaml_avcodec_encode(_ctx, 0);
}


/**** codec ****/

static enum AVCodecID find_codec_id(const char *name)
{
  avcodec_register_all();

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

  avcodec_register_all();

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

  avcodec_register_all();

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
