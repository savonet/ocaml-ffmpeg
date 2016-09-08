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

#define Raise(exn, msg) caml_raise_with_string(*caml_named_value(exn), (msg))
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

// audio context
typedef struct {
  SwrContext *resampler;
  int64_t out_channel_layout;
  int out_nb_channels;
  int out_sample_rate;
  enum AVSampleFormat out_sample_fmt;
  value out_ba;
  enum caml_ba_kind out_ba_kind;
  int out_ba_max_nb_samples;
} audio_t;

// video context
typedef struct {
  uint8_t *dst_data[4];
} video_t;

// subtitle context
typedef struct {
} subtitle_t;

typedef struct {
  int index;
  AVCodecContext *codec;
  AVFrame * frame;
  int got_frame;
  audio_t *audio;
  video_t *video;
  AVSubtitle *subtitle;
} stream_t;

typedef struct {
  AVFormatContext *format;
  AVPacket packet;
  int end_of_file;
  int selected_streams;
  int nb_streams;
  stream_t ** streams;
  stream_t * audio_stream;
  stream_t * video_stream;
  stream_t * subtitle_stream;
} context_t;

#define AUDIO_FRAME_TAG 0
#define VIDEO_FRAME_TAG 1
#define SUBTITLE_FRAME_TAG 2

#define Context_val(v) (*(context_t**)Data_custom_val(v))

static void free_stream(stream_t * stream)
{
  if( ! stream) return;
  
  if(stream->codec) avcodec_free_context(&stream->codec);
  if(stream->frame) av_frame_free(&stream->frame);

  if(stream->audio) {
    if(stream->audio->resampler) swr_free(&stream->audio->resampler);
    if(stream->audio->out_ba) caml_remove_global_root(&stream->audio->out_ba);
    free(stream->audio);
  }

  if(stream->video) {
    if(stream->video->dst_data[0]) av_free(stream->video->dst_data[0]);
    free(stream->video);
  }

  if(stream->subtitle) {
    avsubtitle_free(stream->subtitle);
    free(stream->subtitle);
  }

  free(stream);
}

static void finalize_context(value v)
{
  context_t *ctx = Context_val(v);

  if(ctx->format) avformat_close_input(&ctx->format);
  av_packet_unref(&ctx->packet);

  if(ctx->streams) {
    for(int i = 0; i < ctx->nb_streams; i++) {
      free_stream(ctx->streams[i]);
    }
    free(ctx->streams);
  }
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
/*
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

  /* open input file, and allocate format context */
  if (avformat_open_input(&ctx->format, filename, NULL, NULL) < 0) {
    Raise(EXN_OPEN, filename);
  }

  /* retrieve stream information */
  if (avformat_find_stream_info(ctx->format, NULL) < 0) {
    Raise(EXN_FAILURE, "Stream information not found");
  }

  /* initialize packet, set data to NULL, let the demuxer fill it */
  av_init_packet(&ctx->packet);
  ctx->packet.data = NULL;
  ctx->packet.size = 0;

  CAMLreturn(ans);
}

static stream_t * open_stream_index(context_t *ctx, int index)
{
  int new_nb = index + 1;
  
  if(new_nb > ctx->nb_streams) {
    stream_t ** streams = (stream_t **)realloc(ctx->streams, new_nb * sizeof(stream_t *));
    
    if ( ! streams) {
      Raise(EXN_FAILURE, "Failed to allocate streams array");
    }
    memset(streams + ctx->nb_streams, 0, new_nb - ctx->nb_streams);
    ctx->nb_streams = new_nb;
    ctx->streams = streams;
  }
  
  /* find decoder for the stream */
  AVCodecParameters *dec_param = ctx->format->streams[index]->codecpar;
  enum AVMediaType type = dec_param->codec_type;
  AVCodec *dec = avcodec_find_decoder(dec_param->codec_id);

  if ( ! dec) {
    snprintf(error_msg, ERROR_MSG_SIZE, "Failed to find stream %d %s codec", index, av_get_media_type_string(type));
    Raise(EXN_FAILURE, error_msg);
  }

  stream_t * stream = (stream_t*)calloc(1, sizeof(stream_t));

  if ( ! stream) {
    snprintf(error_msg, ERROR_MSG_SIZE, "Failed to allocate stream %d context", index);
    Raise(EXN_FAILURE, error_msg);
  }

  stream->index = index;

  AVCodecContext *codec = avcodec_alloc_context3(dec);

  if ( ! codec) {
    snprintf(error_msg, ERROR_MSG_SIZE, "Failed to allocate stream %d %s codec context", index, av_get_media_type_string(type));
    Raise(EXN_FAILURE, error_msg);
  }

  stream->codec = codec;

  // initialize the stream parameters with demuxer information
  int ret = avcodec_parameters_to_context(codec, dec_param);

  if (ret < 0) {
    free_stream(stream);
    snprintf(error_msg, ERROR_MSG_SIZE, "Failed to initialize the stream context with the stream parameters : %s", av_err2str(ret));
    Raise(EXN_FAILURE, error_msg);
  }

  /* Init the decoders without reference counting 
     AVDictionary *opts = NULL;
     av_dict_set(&opts, "refcounted_frames", "0", 0);
  */
  ret = avcodec_open2(codec, dec, NULL/*&opts*/);

  if (ret < 0) {
    free_stream(stream);
    snprintf(error_msg, ERROR_MSG_SIZE, "Failed to open stream %d %s codec : %s", index, av_get_media_type_string(type), av_err2str(ret));
    Raise(EXN_FAILURE, error_msg);
  }
  /* dump input information to stderr */
  av_dump_format(ctx->format, index, "un fichier", 0);

  if(type == AVMEDIA_TYPE_AUDIO || type == AVMEDIA_TYPE_VIDEO) {

    stream->frame = av_frame_alloc();

    if ( ! stream->frame) {
      free_stream(stream);
      snprintf(error_msg, ERROR_MSG_SIZE, "Failed to allocate stream %d frame", index);
      Raise(EXN_FAILURE, error_msg);
    }
  }
  
  switch(type) {
  case AVMEDIA_TYPE_AUDIO :
    stream->audio = (audio_t*)calloc(1, sizeof(audio_t));

    if ( ! stream->audio) {
      free_stream(stream);
      snprintf(error_msg, ERROR_MSG_SIZE, "Failed to allocate stream %d audio context", index);
      Raise(EXN_FAILURE, error_msg);
    }

    stream->audio->out_ba_kind = -1;
    stream->audio->out_channel_layout = codec->channel_layout;
    stream->audio->out_nb_channels = av_get_channel_layout_nb_channels(codec->channel_layout);
    stream->audio->out_sample_rate = codec->sample_rate;
    stream->audio->out_sample_fmt = codec->sample_fmt;
    break;
  case AVMEDIA_TYPE_VIDEO :
    stream->video = (video_t*)calloc(1, sizeof(video_t));

    if ( ! stream->video) {
      free_stream(stream);
      snprintf(error_msg, ERROR_MSG_SIZE, "Failed to allocate stream %d video context", index);
      Raise(EXN_FAILURE, error_msg);
    }
    break;
  case AVMEDIA_TYPE_SUBTITLE :
    stream->subtitle = (AVSubtitle*)calloc(1, sizeof(AVSubtitle));

    if ( ! stream->subtitle) {
      free_stream(stream);
      snprintf(error_msg, ERROR_MSG_SIZE, "Failed to allocate stream %d subtitle context", index);
      Raise(EXN_FAILURE, error_msg);
    }
  default :
    free_stream(stream);
    snprintf(error_msg, ERROR_MSG_SIZE, "Failed to allocate stream %d of media type %s", index, av_get_media_type_string(type));
    Raise(EXN_FAILURE, error_msg);
  }

  ctx->streams[index] = stream;

  return stream;
}

static stream_t * open_best_stream(context_t *ctx, enum AVMediaType type)
{
  int index = av_find_best_stream(ctx->format, type, -1, -1, NULL, 0);

  if (index < 0) {
    snprintf(error_msg, ERROR_MSG_SIZE, "Failed to find %s stream", av_get_media_type_string(type));
    Raise(EXN_FAILURE, error_msg);
  }

  return open_stream_index(ctx, index);
}

static void open_audio_stream(context_t * ctx)
{
  ctx->audio_stream = open_best_stream(ctx, AVMEDIA_TYPE_AUDIO);
}

static void open_video_stream(context_t * ctx)
{
  ctx->video_stream = open_best_stream(ctx, AVMEDIA_TYPE_VIDEO);
}

static void open_subtitle_stream(context_t * ctx)
{
  ctx->subtitle_stream = open_best_stream(ctx, AVMEDIA_TYPE_SUBTITLE);
}

CAMLprim value ocaml_ffmpeg_get_audio_in_channel_layout(value _ctx) {
  CAMLparam1(_ctx);
  context_t * ctx = Context_val(_ctx);

  if( ! ctx->audio_stream) open_audio_stream(ctx);

  CAMLreturn(val_of_channel_layout(ctx->audio_stream->codec->channel_layout));
}

CAMLprim value ocaml_ffmpeg_get_audio_in_nb_channels(value _ctx) {
  CAMLparam1(_ctx);
  context_t * ctx = Context_val(_ctx);

  if( ! ctx->audio_stream) open_audio_stream(ctx);

  CAMLreturn(Val_int(av_get_channel_layout_nb_channels(ctx->audio_stream->codec->channel_layout)));
}

CAMLprim value ocaml_ffmpeg_get_audio_in_sample_rate(value _ctx) {
  CAMLparam1(_ctx);
  context_t * ctx = Context_val(_ctx);

  if( ! ctx->audio_stream) open_audio_stream(ctx);

  CAMLreturn(Val_int(ctx->audio_stream->codec->sample_rate));
}

CAMLprim value ocaml_ffmpeg_get_audio_in_sample_format(value _ctx) {
  CAMLparam1(_ctx);
  context_t * ctx = Context_val(_ctx);

  if( ! ctx->audio_stream) open_audio_stream(ctx);

  CAMLreturn(val_of_sample_format(ctx->audio_stream->codec->sample_fmt));
}

CAMLprim value ocaml_ffmpeg_get_audio_out_channel_layout(value _ctx) {
  CAMLparam1(_ctx);
  context_t * ctx = Context_val(_ctx);

  if( ! ctx->audio_stream) open_audio_stream(ctx);

  CAMLreturn(val_of_channel_layout(ctx->audio_stream->audio->out_channel_layout));
}

CAMLprim value ocaml_ffmpeg_get_audio_out_nb_channels(value _ctx) {
  CAMLparam1(_ctx);
  context_t * ctx = Context_val(_ctx);

  if( ! ctx->audio_stream) open_audio_stream(ctx);

  CAMLreturn(Val_int(ctx->audio_stream->audio->out_nb_channels));
}

CAMLprim value ocaml_ffmpeg_get_audio_out_sample_rate(value _ctx) {
  CAMLparam1(_ctx);
  context_t * ctx = Context_val(_ctx);

  if( ! ctx->audio_stream) open_audio_stream(ctx);

  CAMLreturn(Val_int(ctx->audio_stream->audio->out_sample_rate));
}

CAMLprim value ocaml_ffmpeg_get_audio_out_sample_format(value _ctx) {
  CAMLparam1(_ctx);
  context_t * ctx = Context_val(_ctx);

  if( ! ctx->audio_stream) open_audio_stream(ctx);

  CAMLreturn(val_of_sample_format(ctx->audio_stream->audio->out_sample_fmt));
}
    
/**
 */
static void set_audio_resampler(stream_t * stream, int64_t channel_layout, int sample_rate, enum AVSampleFormat sample_fmt)
{
  audio_t * audio = stream->audio;

  if(channel_layout) {
    audio->out_channel_layout = channel_layout;
    audio->out_nb_channels = av_get_channel_layout_nb_channels(channel_layout);
  }

  if(sample_rate) {
    audio->out_sample_rate = sample_rate;
  }

  if(sample_fmt != AV_SAMPLE_FMT_NONE) {
    audio->out_sample_fmt = sample_fmt;
  }
  
  // create resampler context
  audio->resampler = swr_alloc_set_opts(audio->resampler,
					audio->out_channel_layout,     // out_ch_layout
					audio->out_sample_fmt,         // out_sample_fmt
					audio->out_sample_rate,        // out_sample_rate
					stream->codec->channel_layout, // in_ch_layout
					stream->codec->sample_fmt,     // in_sample_fmt
					stream->codec->sample_rate,    // in_sample_rate
					0, NULL);                      // log_offset, log_ctx
  if ( ! audio->resampler) {
    Raise(EXN_FAILURE, "Could not allocate resampler context");
  }

  // initialize the resampling context
  int ret = swr_init(audio->resampler);
  
  if (ret < 0) {
    snprintf(error_msg, ERROR_MSG_SIZE, "Failed to initialize the resampling context : %s", av_err2str(ret));
    Raise(EXN_FAILURE, error_msg);
  }
}

CAMLprim value ocaml_ffmpeg_set_audio_out_format(value _channel_layout, value _sample_rate, value _ctx)
{
  CAMLparam3(_channel_layout, _sample_rate, _ctx);
  context_t * ctx = Context_val(_ctx);

  if( ! ctx->audio_stream) open_audio_stream(ctx);

  audio_t * audio = ctx->audio_stream->audio;
  
  if (Is_block(_channel_layout)) {
    audio->out_channel_layout = channel_layout_of_val(Field(_channel_layout, 0));
  }

  if (Is_block(_sample_rate)) {
    audio->out_sample_rate = Int_val(Field(_sample_rate, 0));
  }

  /* if the resampler is already created, it is freed.
     The next call to convertion will create it with the new format by calling get_out_samples. */
  if(audio->resampler) swr_free(&audio->resampler);
  
  CAMLreturn(Val_unit);
}


static int decode_packet(context_t * ctx, stream_t * stream)
{
  AVPacket * packet = &ctx->packet;
  AVCodecContext * dec = stream->codec;
  int ret, frame_tag = AUDIO_FRAME_TAG;
  
  if(dec->codec_type == AVMEDIA_TYPE_AUDIO ||
     dec->codec_type == AVMEDIA_TYPE_VIDEO) {
  
    ret = avcodec_send_packet(dec, packet);
  
    if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
      snprintf(error_msg, ERROR_MSG_SIZE, "Error decoding %s frame (%s)", av_get_media_type_string(dec->codec_type), av_err2str(ret));
      Raise(EXN_FAILURE, error_msg);
    }
  
    if (ret >= 0) packet->size = 0;
  
    // decode frame
    ret = avcodec_receive_frame(dec, stream->frame);
  
    if(dec->codec_type == AVMEDIA_TYPE_VIDEO) {
      ctx->video_stream = stream;
      frame_tag = VIDEO_FRAME_TAG;
    }
    else ctx->audio_stream = stream;
  }
  else if(dec->codec_type == AVMEDIA_TYPE_SUBTITLE) {
  
    ret = avcodec_decode_subtitle2(dec, stream->subtitle, &stream->got_frame, packet);
  
    if (ret >= 0) packet->size = 0;
  
    ctx->subtitle_stream = stream;
    frame_tag = SUBTITLE_FRAME_TAG;
  }
  
  if(packet->size <= 0) {
    av_packet_unref(packet);
  }
  
  if(ret >= 0) {
    stream->got_frame = 1;
    ret = frame_tag;
  }
  else if (ret == AVERROR_EOF) {
    stream->got_frame = 0;
  }
  else if (ret != AVERROR(EAGAIN)) {
    snprintf(error_msg, ERROR_MSG_SIZE, "Error decoding %s frame (%s)", av_get_media_type_string(dec->codec_type), av_err2str(ret));
    Raise(EXN_FAILURE, error_msg);
  }
    
  return ret;
}

static void read_stream_frame(context_t * ctx, stream_t * stream)
{
  AVPacket * packet = &ctx->packet;
  int stream_index = stream->index;
  int ret = 0;
  
  for(;;) {
    if( ! ctx->end_of_file) {
      if(packet->size <= 0) {
	// read frames from the file
	if(av_read_frame(ctx->format, packet) < 0) {
	  packet->data = NULL;
	  packet->size = 0;
	  ctx->end_of_file = 1;
	  break;
	}
	else if (packet->stream_index != stream_index) {
	  av_packet_unref(packet);
	  continue;
	}
      }
    }
  
    ret = decode_packet(ctx, stream);
  
    if(ret == AVERROR_EOF) {
      caml_raise_constant(*caml_named_value(EXN_EOF));
    }
  
    if(ret >= 0) {
      break;
    }
  }
}

CAMLprim value ocaml_ffmpeg_read_audio(value _ctx)
{
  CAMLparam1(_ctx);
  context_t * ctx = Context_val(_ctx);
  
  if( ! ctx->audio_stream) open_audio_stream(ctx);
  
  read_stream_frame(ctx, ctx->audio_stream);
  
  CAMLreturn(_ctx);
}

CAMLprim value ocaml_ffmpeg_read_video(value _ctx)
{
  CAMLparam1(_ctx);
  context_t * ctx = Context_val(_ctx);

  if( ! ctx->video_stream) open_video_stream(ctx);

  read_stream_frame(ctx, ctx->video_stream);

  CAMLreturn(_ctx);
}

CAMLprim value ocaml_ffmpeg_read_subtitle(value _ctx)
{
  CAMLparam1(_ctx);
  context_t * ctx = Context_val(_ctx);

  if( ! ctx->subtitle_stream) open_subtitle_stream(ctx);

  read_stream_frame(ctx, ctx->subtitle_stream);

  CAMLreturn(_ctx);
}

CAMLprim value ocaml_ffmpeg_read(value _ctx)
{
  CAMLparam1(_ctx);
  CAMLlocal1(ans);
  context_t * ctx = Context_val(_ctx);
  AVPacket * packet = &ctx->packet;
  stream_t ** streams = ctx->streams;
  stream_t * stream = NULL;
  int nb_streams = ctx->nb_streams;
  int frame_tag = AUDIO_FRAME_TAG;

  for(;;) {
    if( ! ctx->end_of_file) {
  
      if(packet->size <= 0) {
	// read frames from the file
	if(av_read_frame(ctx->format, packet) < 0) {
	  packet->data = NULL;
	  packet->size = 0;
	  ctx->end_of_file = 1;
	  continue;
	}
      }
  
      if(packet->stream_index >= nb_streams ||
	 (stream = streams[packet->stream_index]) == NULL) {
  
	if(ctx->selected_streams) {
	  av_packet_unref(packet);
	  continue;
	}
	else {
	  stream = open_stream_index(ctx, packet->stream_index);
	}
      }
    }
    else {
      int i = 0;
      for(; i < nb_streams; i++) {
	if((stream = streams[i]) && stream->got_frame) break;
      }
  
      if(i == nb_streams) {
	caml_raise_constant(*caml_named_value(EXN_EOF));
      }
    }
  
    frame_tag = decode_packet(ctx, stream);
  
    if(frame_tag >= 0) break;
  }
  
  ans = caml_alloc_small(1, frame_tag);
  Field(ans, 0) = _ctx;
  
  CAMLreturn(ans);
}

int get_out_samples(stream_t * stream, enum AVSampleFormat sample_fmt)
{
  audio_t * audio = stream->audio;
  
  /* initialize the resampler if necessary */
  if( ! audio->resampler || audio->out_sample_fmt != sample_fmt) {
    set_audio_resampler(stream, 0, 0, sample_fmt);
  }
  
  /* compute destination number of samples */
  return swr_get_out_samples(audio->resampler, stream->frame->nb_samples);
}

CAMLprim value ocaml_ffmpeg_get_out_samples(value _sample_fmt, value _ctx)
{
  CAMLparam2(_sample_fmt, _ctx);

  context_t * ctx = Context_val(_ctx);

  if( ! ctx->audio_stream) open_audio_stream(ctx);

  stream_t * stream = ctx->audio_stream;
  enum AVSampleFormat sample_fmt = stream->audio->out_sample_fmt;

  if (Is_block(_sample_fmt)) {
    sample_fmt = sample_format_of_val(Field(_sample_fmt, 0));
  }

  CAMLreturn(Val_int(get_out_samples(stream, sample_fmt)));
}

CAMLprim value ocaml_ffmpeg_audio_to_string(value _ctx)
{
  CAMLparam1(_ctx);
  CAMLlocal1(ans);

  context_t * ctx = Context_val(_ctx);
  stream_t * stream = ctx->audio_stream;
  AVFrame *frame = stream->frame;
  audio_t * audio = stream->audio;
  
  int out_nb_samples = get_out_samples(ctx->audio_stream, av_get_packed_sample_fmt(audio->out_sample_fmt));
  
  //  if (out_nb_samples > caml_string_length(v))

  size_t len = out_nb_samples * audio->out_nb_channels * av_get_bytes_per_sample(audio->out_sample_fmt);
  printf("%d\n", len);
  
  ans = caml_alloc_string(len);
  
  char * out_buf = String_val(ans);
  
  // Convert the samples using the resampler.
  printf("Audio_to_string convert %s to %s : ", av_get_sample_fmt_name(frame->format), av_get_sample_fmt_name(audio->out_sample_fmt));
    
  int ret = swr_convert(audio->resampler,
			(uint8_t **) &out_buf,
			out_nb_samples,
			(const uint8_t **) frame->extended_data,
			frame->nb_samples);

  printf("%d / %d\n", ret, out_nb_samples);

  if (ret < 0) {
    snprintf(error_msg, ERROR_MSG_SIZE, "Failed to convert input samples (error '%d')", ret);
    Raise(EXN_FAILURE, error_msg);
  }

  CAMLreturn(ans);
}

CAMLprim value ocaml_ffmpeg_video_to_string(value _ctx)
{
  CAMLparam1(_ctx);
  CAMLlocal1(ans);

  context_t * ctx = Context_val(_ctx);
  stream_t * stream = ctx->audio_stream;
  AVFrame *frame = stream->frame;
  size_t len = frame->nb_samples * av_get_bytes_per_sample(frame->format);

  ans = caml_alloc_string(len);
  memcpy(String_val(ans), frame->extended_data[0], len);

  CAMLreturn(ans);
}

CAMLprim value ocaml_ffmpeg_subtitle_to_string(value _ctx)
{
  CAMLparam1(_ctx);
  CAMLlocal1(ans);

  context_t * ctx = Context_val(_ctx);
  unsigned num_rects = ctx->subtitle_stream->subtitle->num_rects;
  AVSubtitleRect **rects = ctx->subtitle_stream->subtitle->rects;
  size_t len = 0;
  
  for(unsigned i = 0; i < num_rects; i++) len += strlen(rects[i]->text);
    
  ans = caml_alloc_string(len + 1);
  char * dest = String_val(ans);

  for(unsigned i = 0; i < num_rects; i++) strcat(dest, rects[i]->text);

  CAMLreturn(ans);
}

/**
 * Convert the input audio samples into the output sample format.
 * The conversion happens on a per-frame basis.
 */
static value audio_to_bigarray(context_t * ctx, enum AVSampleFormat sample_fmt, enum caml_ba_kind ba_kind)
{
  stream_t * stream = ctx->audio_stream;
  AVFrame * frame = stream->frame;
  audio_t * audio = stream->audio;
  int out_nb_samples = get_out_samples(ctx->audio_stream, sample_fmt);

  if (ba_kind != audio->out_ba_kind
      || out_nb_samples > audio->out_ba_max_nb_samples) {

    if(audio->out_ba) caml_remove_global_root(&audio->out_ba);

    intnat out_size = out_nb_samples * audio->out_nb_channels;
    
    audio->out_ba = caml_ba_alloc(CAML_BA_C_LAYOUT | ba_kind, 1, NULL, &out_size);
    audio->out_ba_kind = ba_kind;
    audio->out_ba_max_nb_samples = out_nb_samples;
    
    caml_register_global_root(&audio->out_ba);
  }

  // Convert the samples using the resampler.
  int ret = swr_convert(audio->resampler,
			(uint8_t **) &Caml_ba_data_val(audio->out_ba),
			audio->out_ba_max_nb_samples,
			(const uint8_t **)frame->extended_data,
			frame->nb_samples);

  if (ret < 0) {
    snprintf(error_msg, ERROR_MSG_SIZE, "Failed to convert input samples (error '%d')", ret);
    Raise(EXN_FAILURE, error_msg);
  }

  Caml_ba_array_val(audio->out_ba)->dim[0] = ret * audio->out_nb_channels;

  return audio->out_ba;
}

CAMLprim value ocaml_ffmpeg_audio_to_float64_bigarray(value _ctx) {
  CAMLparam1(_ctx);
  CAMLreturn(audio_to_bigarray(Context_val(_ctx), AV_SAMPLE_FMT_DBL, CAML_BA_FLOAT64));
}
