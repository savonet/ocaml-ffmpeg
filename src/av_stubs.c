#include <caml/mlvalues.h>
#include <caml/memory.h>
#include <caml/alloc.h>
#include <caml/fail.h>
#include <caml/callback.h>
#include <caml/custom.h>
#include <caml/bigarray.h>
#include <caml/threads.h>

#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libavutil/channel_layout.h>
#include <libavutil/timestamp.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>

#define ERROR_MSG_SIZE 256
static char error_msg[ERROR_MSG_SIZE + 1];

#define EXN_FAILURE "ffmpeg_exn_failure_msg"
#define EXN_OPEN "ffmpeg_exn_open_failure"
#define EXN_EOF "ffmpeg_exn_end_of_file"

#define Raise(exn, msg) caml_raise_with_arg(*caml_named_value(exn), caml_copy_string(msg))
static int init_done = 0;


CAMLprim value ocaml_ffmpeg_init(value unit)
{
  CAMLparam0();

  caml_release_runtime_system();
  /* register all formats and codecs */
  av_register_all();
  caml_acquire_runtime_system();

  CAMLreturn(Val_unit);
}

/**** Context ****/

typedef struct {
  int index;
  AVCodecContext *codec;
} stream_context_t;

// audio context
typedef struct {
  stream_context_t stream;
  SwrContext *resampler;
  int64_t out_channel_layout;
  int out_nb_channels;
  int out_sample_rate;
  enum AVSampleFormat out_sample_fmt;
  value out_ba;
  enum caml_ba_kind out_ba_kind;
  int out_ba_max_nb_samples;
} audio_context_t;

// video context
typedef struct {
  stream_context_t stream;
  uint8_t *dst_data[4];
} video_context_t;

typedef struct {
  AVFormatContext *format;
  AVPacket original_packet;
  AVPacket packet;
  AVFrame *frame;
  int got_frame;
  int end_of_file;
  audio_context_t audio;
  video_context_t video;
} context_t;

#define Context_val(v) (*(context_t**)Data_custom_val(v))

static void finalize_context(value v)
{
  context_t *ctx = Context_val(v);

  if(ctx->audio.stream.codec) avcodec_free_context(&ctx->audio.stream.codec);
  if(ctx->video.stream.codec) avcodec_free_context(&ctx->video.stream.codec);
  if(ctx->format) avformat_close_input(&ctx->format);
  if(ctx->frame) av_frame_free(&ctx->frame);
  if(ctx->audio.resampler) swr_free(&ctx->audio.resampler);
  if(ctx->audio.out_ba) caml_remove_global_root(&ctx->audio.out_ba);
  if(ctx->video.dst_data[0]) av_free(ctx->video.dst_data[0]);
 
  free(ctx);
}

static struct custom_operations context_ops =
  {
    "ocaml_ffmpeg_context",
    finalize_context,
    custom_compare_default,
    custom_hash_default,
    custom_serialize_default,
    custom_deserialize_default
  };

static value value_of_context(context_t *context)
{
  if (!context)
    Raise(EXN_FAILURE, "Empty context");

  value ans = caml_alloc_custom(&context_ops, sizeof(context_t*), 0, 1);
  Context_val(ans) = context;
  return ans;
}

/**** Channel layout ****/

#define channel_layouts_len 28
static const uint64_t channel_layouts[channel_layouts_len] = {
  AV_CH_LAYOUT_MONO,
  AV_CH_LAYOUT_STEREO,
  AV_CH_LAYOUT_2POINT1,
  AV_CH_LAYOUT_2_1,
  AV_CH_LAYOUT_SURROUND,
  AV_CH_LAYOUT_3POINT1,
  AV_CH_LAYOUT_4POINT0,
  AV_CH_LAYOUT_4POINT1,
  AV_CH_LAYOUT_2_2,
  AV_CH_LAYOUT_QUAD,
  AV_CH_LAYOUT_5POINT0,
  AV_CH_LAYOUT_5POINT1,
  AV_CH_LAYOUT_5POINT0_BACK,
  AV_CH_LAYOUT_5POINT1_BACK,
  AV_CH_LAYOUT_6POINT0,
  AV_CH_LAYOUT_6POINT0_FRONT,
  AV_CH_LAYOUT_HEXAGONAL,
  AV_CH_LAYOUT_6POINT1,
  AV_CH_LAYOUT_6POINT1_BACK,
  AV_CH_LAYOUT_6POINT1_FRONT,
  AV_CH_LAYOUT_7POINT0,
  AV_CH_LAYOUT_7POINT0_FRONT,
  AV_CH_LAYOUT_7POINT1,
  AV_CH_LAYOUT_7POINT1_WIDE,
  AV_CH_LAYOUT_7POINT1_WIDE_BACK,
  AV_CH_LAYOUT_OCTAGONAL,
  AV_CH_LAYOUT_HEXADECAGONAL,
  AV_CH_LAYOUT_STEREO_DOWNMIX
};

static uint64_t channel_layout_of_val(value val)
{
  return channel_layouts[Int_val(val)];
}

static value val_of_channel_layout(uint64_t cl)
{
  for (int i = 0; i < channel_layouts_len; i++) {
    if (cl == channel_layouts[i])
      return Val_int(i);
  }
  printf("error in channel layout : %llu\n", cl);
  return Val_int(0);
}


/**** Sample format ****/

#define sample_formats_len 11
static const enum AVSampleFormat sample_formats[sample_formats_len] = {
  AV_SAMPLE_FMT_NONE,
  AV_SAMPLE_FMT_U8,	
  AV_SAMPLE_FMT_S16,	
  AV_SAMPLE_FMT_S32,	
  AV_SAMPLE_FMT_FLT,	
  AV_SAMPLE_FMT_DBL,	
  AV_SAMPLE_FMT_U8P,	
  AV_SAMPLE_FMT_S16P,	
  AV_SAMPLE_FMT_S32P,	
  AV_SAMPLE_FMT_FLTP,	
  AV_SAMPLE_FMT_DBLP
};

static enum AVSampleFormat sample_format_of_val(value val)
{
  return sample_formats[Int_val(val)];
}

static value val_of_sample_format(enum AVSampleFormat sf)
{
  for (int i = 0; i < sample_formats_len; i++) {
    if (sf == sample_formats[i])
      return Val_int(i);
  }
  printf("error in sample format : %d\n", sf);
  return Val_int(0);
}

static const enum caml_ba_kind bigarray_kinds[sample_formats_len] = {
  CAML_BA_KIND_MASK,
  CAML_BA_UINT8,
  CAML_BA_SINT16,	
  CAML_BA_INT32,	
  CAML_BA_FLOAT32,
  CAML_BA_FLOAT64,
  CAML_BA_UINT8,
  CAML_BA_SINT16,
  CAML_BA_INT32,	
  CAML_BA_FLOAT32,
  CAML_BA_FLOAT64
};
/*
  static enum caml_ba_kind bigarray_kind_of_sample_format_val(value val)
  {
  return bigarray_kinds[Int_val(val)];
  }

  static enum caml_ba_kind bigarray_kind_of_sample_format(enum AVSampleFormat sf)
  {
  for (int i = 0; i < sample_formats_len; i++)
  {
  if (sf == sample_formats[i])
  return bigarray_kinds[i];
  }
  return CAML_BA_KIND_MASK;
  }
*/

CAMLprim value ocaml_ffmpeg_open_input(value _filename)
{
  CAMLparam1(_filename);
  CAMLlocal1(ans);
  const char *filename = String_val(_filename);

  if( ! init_done) {
    av_register_all();
    init_done = 1;
  }

  context_t *ctx = (context_t*)calloc(1, sizeof(context_t));

  ans = value_of_context(ctx);
  ctx->audio.out_sample_fmt = AV_SAMPLE_FMT_NONE;
  ctx->audio.out_ba_kind = -1;

  /* open input file, and allocate format context */
  if (avformat_open_input(&ctx->format, filename, NULL, NULL) < 0) {
    Raise(EXN_OPEN, filename);
  }

  /* retrieve stream information */
  if (avformat_find_stream_info(ctx->format, NULL) < 0) {
    Raise(EXN_FAILURE, "Stream information not found");
  }

  ctx->frame = av_frame_alloc();

  if ( ! ctx->frame) {
    Raise(EXN_FAILURE, "Failed to allocate frame");
  }

  /* initialize packet, set data to NULL, let the demuxer fill it */
  av_init_packet(&ctx->packet);
  ctx->packet.data = NULL;
  ctx->packet.size = 0;

  CAMLreturn(ans);
}

static void open_stream_codec(stream_context_t *stream_ctx, AVFormatContext *fmt_ctx, enum AVMediaType type)
{
  stream_ctx->index = av_find_best_stream(fmt_ctx, type, -1, -1, NULL, 0);

  if (stream_ctx->index < 0) {
    snprintf(error_msg, ERROR_MSG_SIZE, "Failed to find %s stream", av_get_media_type_string(type));
    Raise(EXN_FAILURE, error_msg);
  }

  /* find decoder for the stream */
  AVCodecParameters *dec_param = fmt_ctx->streams[stream_ctx->index]->codecpar;
  AVCodec *dec = avcodec_find_decoder(dec_param->codec_id);

  if ( ! dec) {
    snprintf(error_msg, ERROR_MSG_SIZE, "Failed to find %s codec", av_get_media_type_string(type));
    Raise(EXN_FAILURE, error_msg);
  }

  stream_ctx->codec = avcodec_alloc_context3(dec);

  if ( ! stream_ctx->codec) {
    snprintf(error_msg, ERROR_MSG_SIZE, "Failed to allocate %s codec context", av_get_media_type_string(type));
    Raise(EXN_FAILURE, error_msg);
  }

  // initialize the stream parameters with demuxer information
  int ret = avcodec_parameters_to_context(stream_ctx->codec, dec_param);

  if (ret < 0) {
    snprintf(error_msg, ERROR_MSG_SIZE, "Failed to initialize the stream context with the stream parameters : %s", av_err2str(ret));
    Raise(EXN_FAILURE, error_msg);
  }

  /* Init the decoders without reference counting 
     AVDictionary *opts = NULL;
     av_dict_set(&opts, "refcounted_frames", "0", 0);
  */
  ret = avcodec_open2(stream_ctx->codec, dec, NULL/*&opts*/);

  if (ret < 0) {
    snprintf(error_msg, ERROR_MSG_SIZE, "Failed to open %s codec : %s", av_get_media_type_string(type), av_err2str(ret));
    Raise(EXN_FAILURE, error_msg);
  }
  /* dump input information to stderr */
  av_dump_format(fmt_ctx, stream_ctx->index, "un fichier", 0);
}

static void open_audio_stream_codec(context_t * ctx)
{
  open_stream_codec(&ctx->audio.stream, ctx->format, AVMEDIA_TYPE_AUDIO);

  ctx->audio.out_channel_layout = ctx->audio.stream.codec->channel_layout;
  ctx->audio.out_nb_channels = av_get_channel_layout_nb_channels(ctx->audio.out_channel_layout);
  ctx->audio.out_sample_rate = ctx->audio.stream.codec->sample_rate;
  ctx->audio.out_sample_fmt = ctx->audio.stream.codec->sample_fmt;
}

static void open_video_stream_codec(context_t * ctx)
{
  open_stream_codec(&ctx->video.stream, ctx->format, AVMEDIA_TYPE_VIDEO);
}

CAMLprim value ocaml_ffmpeg_get_audio_in_channel_layout(value _ctx) {
  CAMLparam1(_ctx);
  context_t * ctx = Context_val(_ctx);

  if( ! ctx->audio.stream.codec) open_audio_stream_codec(ctx);

  CAMLreturn(val_of_channel_layout(ctx->audio.stream.codec->channel_layout));
}

CAMLprim value ocaml_ffmpeg_get_audio_in_nb_channels(value _ctx) {
  CAMLparam1(_ctx);
  context_t * ctx = Context_val(_ctx);

  if( ! ctx->audio.stream.codec) open_audio_stream_codec(ctx);

  CAMLreturn(Val_int(av_get_channel_layout_nb_channels(ctx->audio.stream.codec->channel_layout)));
}

CAMLprim value ocaml_ffmpeg_get_audio_in_sample_rate(value _ctx) {
  CAMLparam1(_ctx);
  context_t * ctx = Context_val(_ctx);

  if( ! ctx->audio.stream.codec) open_audio_stream_codec(ctx);

  CAMLreturn(Val_int(ctx->audio.stream.codec->sample_rate));
}

CAMLprim value ocaml_ffmpeg_get_audio_in_sample_format(value _ctx) {
  CAMLparam1(_ctx);
  context_t * ctx = Context_val(_ctx);

  if( ! ctx->audio.stream.codec) open_audio_stream_codec(ctx);

  CAMLreturn(val_of_sample_format(ctx->audio.stream.codec->sample_fmt));
}

CAMLprim value ocaml_ffmpeg_get_audio_out_channel_layout(value _ctx) {
  CAMLparam1(_ctx);
  context_t * ctx = Context_val(_ctx);

  if( ! ctx->audio.stream.codec) open_audio_stream_codec(ctx);

  CAMLreturn(val_of_channel_layout(ctx->audio.out_channel_layout));
}

CAMLprim value ocaml_ffmpeg_get_audio_out_nb_channels(value _ctx) {
  CAMLparam1(_ctx);
  context_t * ctx = Context_val(_ctx);

  if( ! ctx->audio.stream.codec) open_audio_stream_codec(ctx);

  CAMLreturn(Val_int(ctx->audio.out_nb_channels));
}

CAMLprim value ocaml_ffmpeg_get_audio_out_sample_rate(value _ctx) {
  CAMLparam1(_ctx);
  context_t * ctx = Context_val(_ctx);

  if( ! ctx->audio.stream.codec) open_audio_stream_codec(ctx);

  CAMLreturn(Val_int(ctx->audio.out_sample_rate));
}

CAMLprim value ocaml_ffmpeg_get_audio_out_sample_format(value _ctx) {
  CAMLparam1(_ctx);
  context_t * ctx = Context_val(_ctx);

  if( ! ctx->audio.stream.codec) open_audio_stream_codec(ctx);

  CAMLreturn(val_of_sample_format(ctx->audio.out_sample_fmt));
}
    
/**
 */
static void set_audio_resampler_context(context_t * ctx, int64_t channel_layout, int sample_rate, enum AVSampleFormat sample_fmt)
{
  if( ! ctx->audio.stream.codec) open_audio_stream_codec(ctx);

  if(channel_layout) {
    ctx->audio.out_channel_layout = channel_layout;
    ctx->audio.out_nb_channels = av_get_channel_layout_nb_channels(channel_layout);
  }

  if(sample_rate) {
    ctx->audio.out_sample_rate = sample_rate;
  }

  if(sample_fmt != AV_SAMPLE_FMT_NONE) {
    ctx->audio.out_sample_fmt = sample_fmt;
  }
  
  /* create resampler context */
  ctx->audio.resampler = swr_alloc_set_opts(ctx->audio.resampler,
					    ctx->audio.out_channel_layout,           // out_ch_layout
					    ctx->audio.out_sample_fmt,               // out_sample_fmt
					    ctx->audio.out_sample_rate,              // out_sample_rate
					    ctx->audio.stream.codec->channel_layout, // in_ch_layout
					    ctx->audio.stream.codec->sample_fmt,     // in_sample_fmt
					    ctx->audio.stream.codec->sample_rate,    // in_sample_rate
					    0, NULL);                                // log_offset, log_ctx
  if ( ! ctx->audio.resampler) {
    Raise(EXN_FAILURE, "Could not allocate resampler context");
  }

  /* initialize the resampling context */
  int ret = swr_init(ctx->audio.resampler);
  
  if (ret < 0) {
    snprintf(error_msg, ERROR_MSG_SIZE, "Failed to initialize the resampling context : %s", av_err2str(ret));
    Raise(EXN_FAILURE, error_msg);
  }
}

CAMLprim value ocaml_ffmpeg_set_audio_out_format(value _channel_layout, value _sample_rate, value _ctx)
{
  CAMLparam3(_channel_layout, _sample_rate, _ctx);
  context_t * ctx = Context_val(_ctx);

  if( ! ctx->audio.stream.codec) open_audio_stream_codec(ctx);

  if (Is_block(_channel_layout)) {
    ctx->audio.out_channel_layout = channel_layout_of_val(Field(_channel_layout, 0));
  }

  if (Is_block(_sample_rate)) {
    ctx->audio.out_sample_rate = Int_val(Field(_sample_rate, 0));
  }

  /* if the resampler is already created, it is freed.
     The next call to convertion will create it with the new format by calling get_out_samples. */
  if(ctx->audio.resampler) swr_free(&ctx->audio.resampler);
  
  CAMLreturn(Val_unit);
}


CAMLprim value ocaml_ffmpeg_read_audio_frame_deprec(value _ctx)
{
  CAMLparam1(_ctx);
  context_t * ctx = Context_val(_ctx);

  if( ! ctx->audio.stream.codec) open_audio_stream_codec(ctx);

  /* read frames from the file */
  int audio_stream_index = ctx->audio.stream.index;
  AVPacket * packet = &ctx->packet;
  int ret = 0, decoded = 0;
  do {
    if( ! ctx->end_of_file && packet->size <= 0) {
      while (1) {
	if(av_read_frame(ctx->format, packet) < 0) {
	  packet->data = NULL;
	  packet->size = 0;
	  ctx->end_of_file = 1;
	  break;
	}

	if (packet->stream_index == audio_stream_index) {
	  ctx->original_packet = *packet;
	  break;
	}

	av_packet_unref(packet);
      }
    }
    /* decode audio frame */
    decoded = avcodec_decode_audio4(ctx->audio.stream.codec, ctx->frame, &ctx->got_frame, packet);
    //avcodec_receive_frame
    if (ctx->end_of_file) {
      if( ! ctx->got_frame) {
	caml_raise_constant(*caml_named_value(EXN_EOF));
      }
    }
    else {
      if (decoded < 0) {
	snprintf(error_msg, ERROR_MSG_SIZE, "Error decoding audio frame (%s)", av_err2str(ret));
	Raise(EXN_FAILURE, error_msg);
      }
      else if(decoded > 0) {
	/* Some audio decoders decode only part of the packet, and have to be
	 * called again with the remainder of the packet data.
	 * Sample: fate-suite/lossless-audio/luckynight-partial.shn
	 * Also, some decoders might over-read the packet. */
	decoded = FFMIN(decoded, packet->size);

	packet->data += decoded;
	packet->size -= decoded;

	if(packet->size <= 0) {
	  av_packet_unref(&ctx->original_packet);
	}
      }
    }
  } while( ! ctx->got_frame);

  CAMLreturn(_ctx);
}

/**
 */
static void read_stream_frame(context_t * ctx, AVCodecContext *dec, int stream_index)
{
  AVPacket * packet = &ctx->packet;
  int ret = 0;
  do {
    if( ! ctx->end_of_file) {
      if(packet->size <= 0) {

	while (1) {
	  // read frames from the file
	  if(av_read_frame(ctx->format, packet) < 0) {
	    packet->data = NULL;
	    packet->size = 0;
	    ctx->end_of_file = 1;
	    break;
	  }

	  if (packet->stream_index == stream_index) {
	    ctx->original_packet = *packet;
	    break;
	  }

	  av_packet_unref(packet);
	}
      }

      ret = avcodec_send_packet(dec, packet);

      if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
	snprintf(error_msg, ERROR_MSG_SIZE, "Error decoding %s frame (%s)", av_get_media_type_string(dec->codec_type), av_err2str(ret));
	Raise(EXN_FAILURE, error_msg);
      }

      if (ret >= 0) packet->size = 0;
    }
    // decode frame
    ret = avcodec_receive_frame(dec, ctx->frame);

    if (ret == AVERROR_EOF) {
      caml_raise_constant(*caml_named_value(EXN_EOF));
    }

    if (ret < 0 && ret != AVERROR(EAGAIN)) {
      snprintf(error_msg, ERROR_MSG_SIZE, "Error decoding %s frame (%s)", av_get_media_type_string(dec->codec_type), av_err2str(ret));
      Raise(EXN_FAILURE, error_msg);
    }
    else if(ret >= 0) {
      if(packet->size <= 0) {
	av_packet_unref(&ctx->original_packet);
      }
      break;
    }
  } while(1);
}

CAMLprim value ocaml_ffmpeg_read_audio_frame(value _ctx)
{
  CAMLparam1(_ctx);
  context_t * ctx = Context_val(_ctx);

  if( ! ctx->audio.stream.codec) open_audio_stream_codec(ctx);

  read_stream_frame(ctx, ctx->audio.stream.codec, ctx->audio.stream.index);

  CAMLreturn(_ctx);
}

CAMLprim value ocaml_ffmpeg_read_video_frame(value _ctx)
{
  CAMLparam1(_ctx);
  context_t * ctx = Context_val(_ctx);

  if( ! ctx->video.stream.codec) open_video_stream_codec(ctx);

  read_stream_frame(ctx, ctx->video.stream.codec, ctx->video.stream.index);

  CAMLreturn(_ctx);
}

/*
  CAMLprim value ocaml_ffmpeg_read_frame(value _ctx)
  {
  CAMLparam1(_ctx);
  context_t * ctx = Context_val(_ctx);

  if (ctx->end_of_file && ! ctx->got_frame) {
  caml_raise_constant(*caml_named_value(EXN_EOF));
  }

  // read frames from the file
  int stream_index = ctx->audio.stream.index;
  AVPacket * packet = &ctx->packet;
  int ret = 0, decoded = 0;

  if( ! ctx->end_of_file
  && (packet->stream_index != stream_index || packet->size <= 0))
  {
  av_packet_unref(&ctx->original_packet);

  ret = av_read_frame(ctx->format, packet);

  ctx->original_packet = *packet;

  if (ret >= 0)
  {
  if (packet->stream_index == stream_index) {
  // decode audio frame
  decoded = avcodec_decode_audio4(ctx->audio.stream.codec, ctx->frame, &ctx->got_frame, packet);
  }
  }
  else {
  ctx->end_of_file = 1;
  }
  }
  else
  {
  decoded = avcodec_decode_audio4(ctx->audio.stream.codec, ctx->frame, &ctx->got_frame, packet);
  }

  if (decoded < 0) {
  snprintf(error_msg, ERROR_MSG_SIZE, "Error decoding audio frame (%s)", av_err2str(ret));
  Raise(EXN_FAILURE, error_msg);
  }
  else if(decoded > 0)
  {
  decoded = FFMIN(decoded, packet->size);

  packet->data += decoded;
  packet->size -= decoded;
  }

  CAMLreturn(_ctx);
  }
*/
int get_out_samples(context_t * ctx, enum AVSampleFormat sample_fmt)
{
  /* initialize the resampler if necessary */
  if( ! ctx->audio.resampler || ctx->audio.out_sample_fmt != sample_fmt) {
    set_audio_resampler_context(ctx, 0, 0, sample_fmt);
  }
  
  /* compute destination number of samples */
  return swr_get_out_samples(ctx->audio.resampler, ctx->frame->nb_samples);
}

CAMLprim value ocaml_ffmpeg_get_out_samples(value _sample_fmt, value _ctx)
{
  CAMLparam2(_sample_fmt, _ctx);

  context_t * ctx = Context_val(_ctx);
  enum AVSampleFormat sample_fmt = ctx->audio.out_sample_fmt;

  if (Is_block(_sample_fmt)) {
    sample_fmt = sample_format_of_val(Field(_sample_fmt, 0));
  }

  CAMLreturn(Val_int(get_out_samples(ctx, sample_fmt)));
}

CAMLprim value ocaml_ffmpeg_to_string(value _ctx)
{
  CAMLparam1(_ctx);
  CAMLlocal1(ans);

  context_t * ctx = Context_val(_ctx);
  AVFrame *frame = ctx->frame;
  size_t len = frame->nb_samples * av_get_bytes_per_sample(frame->format);

  ans = caml_alloc_string(len);
  memcpy(String_val(ans), frame->extended_data[0], len);

  CAMLreturn(ans);
}

/**
 * Convert the input audio samples into the output sample format.
 * The conversion happens on a per-frame basis.
 */
static value convert_to_bigarray(context_t * ctx, enum AVSampleFormat sample_fmt, enum caml_ba_kind ba_kind)
{
  int out_nb_samples = get_out_samples(ctx, sample_fmt);

  if (ba_kind != ctx->audio.out_ba_kind
      || out_nb_samples > ctx->audio.out_ba_max_nb_samples) {

    if(ctx->audio.out_ba) caml_remove_global_root(&ctx->audio.out_ba);

    intnat out_size = out_nb_samples * ctx->audio.out_nb_channels;
    
    ctx->audio.out_ba = caml_ba_alloc(CAML_BA_C_LAYOUT | ba_kind, 1, NULL, &out_size);
    ctx->audio.out_ba_kind = ba_kind;
    ctx->audio.out_ba_max_nb_samples = out_nb_samples;
    
    caml_register_global_root(&ctx->audio.out_ba);
  }

  // Convert the samples using the resampler.
  int ret = swr_convert(ctx->audio.resampler,
			(uint8_t **) &Caml_ba_data_val(ctx->audio.out_ba),
			ctx->audio.out_ba_max_nb_samples,
			ctx->frame->extended_data,
			ctx->frame->nb_samples);

  if (ret < 0) {
    snprintf(error_msg, ERROR_MSG_SIZE, "Failed to convert input samples (error '%d')", ret);
    Raise(EXN_FAILURE, error_msg);
  }

  Caml_ba_array_val(ctx->audio.out_ba)->dim[0] = ret * ctx->audio.out_nb_channels;

  return ctx->audio.out_ba;
}

CAMLprim value ocaml_ffmpeg_to_float64_bigarray(value _ctx) {
  CAMLparam1(_ctx);
  CAMLreturn(convert_to_bigarray(Context_val(_ctx), AV_SAMPLE_FMT_DBL, CAML_BA_FLOAT64));
}
