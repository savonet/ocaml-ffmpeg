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
#include <libavutil/timestamp.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>

#include <avutil_stubs.h>

#define ERROR_MSG_SIZE 256
static char error_msg[ERROR_MSG_SIZE + 1];

#define EXN_FAILURE "ffmpeg_exn_failure_msg"
#define EXN_OPEN "ffmpeg_exn_open_failure"
#define EXN_EOF "ffmpeg_exn_end_of_file"

#define Raise(exn, msg) caml_raise_with_string(*caml_named_value(exn), (msg))
static int init_done = 0;


/**** Context ****/

typedef struct {
  int index;
  AVCodecContext *codec;
  value frame;
  int got_frame;
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
} av_t;

#define AUDIO_FRAME_TAG 0
#define VIDEO_FRAME_TAG 1
#define SUBTITLE_FRAME_TAG 2

#define Av_val(v) (*(av_t**)Data_custom_val(v))

static void free_stream(stream_t * stream)
{
  if( ! stream) return;
  
  if(stream->codec) avcodec_free_context(&stream->codec);
  if(stream->frame) caml_remove_global_root(&stream->frame);

  if(stream->subtitle) {
    avsubtitle_free(stream->subtitle);
    free(stream->subtitle);
  }

  free(stream);
}

static void finalize_av(value v)
{
  av_t *av = Av_val(v);

  if(av->format) avformat_close_input(&av->format);
  av_packet_unref(&av->packet);

  if(av->streams) {
    for(int i = 0; i < av->nb_streams; i++) {
      free_stream(av->streams[i]);
    }
    free(av->streams);
  }
  free(av);
}

static struct custom_operations av_ops =
  {
    "ocaml_av_context",
    finalize_av,
    custom_compare_default,
    custom_hash_default,
    custom_serialize_default,
    custom_deserialize_default
  };

static value Val_av(av_t *av)
{
  if (!av)
    Raise(EXN_FAILURE, "Empty context");

  value ans = caml_alloc_custom(&av_ops, sizeof(av_t*), 0, 1);
  Av_val(ans) = av;
  return ans;
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

CAMLprim value ocaml_av_open_input(value _filename)
{
  CAMLparam1(_filename);
  CAMLlocal1(ans);
  int ret;

  if( ! init_done) {
    caml_release_runtime_system();
    av_register_all();
    caml_acquire_runtime_system();
    init_done = 1;
  }

  av_t *av = (av_t*)calloc(1, sizeof(av_t));

  ans = Val_av(av);

  // open input file, and allocate format context
  size_t filename_length = caml_string_length(_filename) + 1;
  char *filename = (char*)malloc(filename_length);
  memcpy(filename, String_val(_filename), filename_length);

  caml_release_runtime_system();
  ret = avformat_open_input(&av->format, filename, NULL, NULL);
  caml_acquire_runtime_system();

  free(filename);
  
  if (ret < 0) {
    Raise(EXN_OPEN, String_val(_filename));
  }

  // retrieve stream information
  caml_release_runtime_system();
  ret = avformat_find_stream_info(av->format, NULL);
  caml_acquire_runtime_system();

  if (ret < 0) {
    Raise(EXN_FAILURE, "Stream information not found");
  }

  // initialize packet, set data to NULL, let the demuxer fill it
  av_init_packet(&av->packet);
  av->packet.data = NULL;
  av->packet.size = 0;

  CAMLreturn(ans);
}

CAMLprim value ocaml_av_get_metadata(value _av) {
  CAMLparam1(_av);
  CAMLlocal3(pair, cons, list);

  av_t * av = Av_val(_av);
  AVDictionary * metadata = av->format->metadata;
  AVDictionaryEntry *tag = NULL;

  list = Val_emptylist;

  while ((tag = av_dict_get(metadata, "", tag, AV_DICT_IGNORE_SUFFIX)))
    {
      pair = caml_alloc_tuple(2);
      Store_field(pair, 0, caml_copy_string(tag->key));
      Store_field(pair, 1, caml_copy_string(tag->value));

      cons = caml_alloc(2, 0);

      Store_field(cons, 0, pair);  // head
      Store_field(cons, 1, list);  // tail

      list = cons;
    }

  CAMLreturn(list);
}

static stream_t * open_stream_index(av_t *av, int index)
{
  int new_nb = index + 1;
  
  if(new_nb > av->nb_streams) {
    stream_t ** streams = (stream_t **)realloc(av->streams, new_nb * sizeof(stream_t *));
    
    if ( ! streams) {
      Raise(EXN_FAILURE, "Failed to allocate streams array");
    }
    memset(streams + av->nb_streams, 0, new_nb - av->nb_streams);
    av->nb_streams = new_nb;
    av->streams = streams;
  }
  
  // find decoder for the stream
  AVCodecParameters *dec_param = av->format->streams[index]->codecpar;
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

  if(type == AVMEDIA_TYPE_AUDIO || type == AVMEDIA_TYPE_VIDEO) {

    AVFrame frame = av_frame_alloc();

    if ( ! frame) {
      free_stream(stream);
      snprintf(error_msg, ERROR_MSG_SIZE, "Failed to allocate stream %d frame", index);
      Raise(EXN_FAILURE, error_msg);
    }

    caml_register_global_root(&stream->frame);
    stream->frame = value_of_frame(frame); 
  }
  
  switch(type) {
  case AVMEDIA_TYPE_AUDIO :
    break;
  case AVMEDIA_TYPE_VIDEO :
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

  av->streams[index] = stream;

  return stream;
}

static stream_t * open_best_stream(av_t *av, enum AVMediaType type)
{
  int index = av_find_best_stream(av->format, type, -1, -1, NULL, 0);

  if (index < 0) {
    snprintf(error_msg, ERROR_MSG_SIZE, "Failed to find %s stream", av_get_media_type_string(type));
    Raise(EXN_FAILURE, error_msg);
  }

  return open_stream_index(av, index);
}

static void open_audio_stream(av_t * av)
{
  av->audio_stream = open_best_stream(av, AVMEDIA_TYPE_AUDIO);
}

static void open_video_stream(av_t * av)
{
  av->video_stream = open_best_stream(av, AVMEDIA_TYPE_VIDEO);
}

static void open_subtitle_stream(av_t * av)
{
  av->subtitle_stream = open_best_stream(av, AVMEDIA_TYPE_SUBTITLE);
}

CAMLprim value ocaml_av_get_audio_channel_layout(value _av) {
  CAMLparam1(_av);
  av_t * av = Av_val(_av);

  if( ! av->audio_stream) open_audio_stream(av);

  CAMLreturn(Val_channelLayout(av->audio_stream->codec->channel_layout));
}

CAMLprim value ocaml_av_get_audio_nb_channels(value _av) {
  CAMLparam1(_av);
  av_t * av = Av_val(_av);

  if( ! av->audio_stream) open_audio_stream(av);

  CAMLreturn(Val_int(av_get_channel_layout_nb_channels(av->audio_stream->codec->channel_layout)));
}

CAMLprim value ocaml_av_get_audio_sample_rate(value _av) {
  CAMLparam1(_av);
  av_t * av = Av_val(_av);

  if( ! av->audio_stream) open_audio_stream(av);

  CAMLreturn(Val_int(av->audio_stream->codec->sample_rate));
}

CAMLprim value ocaml_av_get_audio_sample_format(value _av) {
  CAMLparam1(_av);
  av_t * av = Av_val(_av);

  if( ! av->audio_stream) open_audio_stream(av);

  CAMLreturn(Val_sampleFormat(av->audio_stream->codec->sample_fmt));
}

CAMLprim value ocaml_av_create_resample(value _out_channel_layout, value _out_sample_fmt, value _out_sample_rate, value _av)
{
  CAMLparam4(_out_channel_layout, _out_sample_fmt, _out_sample_rate, _av);
  CAMLlocal1(ans);
  av_t * av = Av_val(_av);

  if( ! av->audio_stream) open_audio_stream(av);

  AVCodecContext *codec = av->audio_stream->codec;
  
  int64_t in_channel_layout = codec->channel_layout;
  enum AVSampleFormat in_sample_fmt = codec->sample_fmt;
  int in_sample_rate = codec->sample_rate;

  int64_t out_channel_layout = in_channel_layout;
  enum AVSampleFormat out_sample_fmt = in_sample_fmt;
  int out_sample_rate = in_sample_rate;

  if (Is_block(_channel_layout)) {
    out_channel_layout = ChannelLayout_val(Field(_channel_layout, 0));
  }

  if (Is_block(_sample_fmt)) {
    out_sample_fmt = SampleFormat_val(Field(_sample_fmt, 0));
  }

  if (Is_block(_sample_rate)) {
    out_sample_rate = Int_val(Field(_sample_rate, 0));
  }

  swr_t * swr = swresample_create(in_channel_layout, in_sample_fmt, in_sample_rate,
				  out_channel_layout, out_sample_fmt, out_sample_rate);
  ans = value_of_swr(swr);
  
  CAMLreturn(ans);
}


static int decode_packet(av_t * av, stream_t * stream)
{
  AVPacket * packet = &av->packet;
  AVFrame *frame = Frame_val(stream->frame);
  AVCodecContext * dec = stream->codec;
  int ret, frame_tag = AUDIO_FRAME_TAG;
  
  if(dec->codec_type == AVMEDIA_TYPE_AUDIO ||
     dec->codec_type == AVMEDIA_TYPE_VIDEO) {
  
    caml_release_runtime_system();
    ret = avcodec_send_packet(dec, packet);
  
    if (ret >= 0 || ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
  
      if (ret >= 0) packet->size = 0;
  
      // decode frame
      ret = avcodec_receive_frame(dec, frame);
    }
    caml_acquire_runtime_system();
  
    if(dec->codec_type == AVMEDIA_TYPE_VIDEO) {
      av->video_stream = stream;
      frame_tag = VIDEO_FRAME_TAG;
    }
    else av->audio_stream = stream;
  }
  else if(dec->codec_type == AVMEDIA_TYPE_SUBTITLE) {
  
    caml_release_runtime_system();
    ret = avcodec_decode_subtitle2(dec, stream->subtitle, &stream->got_frame, packet);
    caml_acquire_runtime_system();
  
    if (ret >= 0) packet->size = 0;
  
    av->subtitle_stream = stream;
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

static void read_stream_frame(av_t * av, stream_t * stream)
{
  AVPacket * packet = &av->packet;
  int stream_index = stream->index;
  
  for(int ret = -1; ret < 0;) {
    if( ! av->end_of_file) {

      if(packet->size <= 0) {
	// read frames from the file
	caml_release_runtime_system();
	int ret = av_read_frame(av->format, packet);
	caml_acquire_runtime_system();

	if(ret < 0) {
	  packet->data = NULL;
	  packet->size = 0;
	  av->end_of_file = 1;
	}
	else if (packet->stream_index != stream_index) {
	  av_packet_unref(packet);
	  continue;
	}
      }
    }
  
    ret = decode_packet(av, stream);
  
    if(ret == AVERROR_EOF) {
      caml_raise_constant(*caml_named_value(EXN_EOF));
    }
  }
}

CAMLprim value ocaml_av_read_audio(value _av)
{
  CAMLparam1(_av);
  av_t * av = Av_val(_av);
  
  if( ! av->audio_stream) open_audio_stream(av);
  
  read_stream_frame(av, av->audio_stream);
  
  CAMLreturn(av->audio_stream->frame);
}

CAMLprim value ocaml_av_read_video(value _av)
{
  CAMLparam1(_av);
  av_t * av = Av_val(_av);

  if( ! av->video_stream) open_video_stream(av);

  read_stream_frame(av, av->video_stream);

  CAMLreturn(av->video_stream->frame);
}

CAMLprim value ocaml_av_read_subtitle(value _av)
{
  CAMLparam1(_av);
  av_t * av = Av_val(_av);

  if( ! av->subtitle_stream) open_subtitle_stream(av);

  read_stream_frame(av, av->subtitle_stream);

  CAMLreturn(_av);
}

CAMLprim value ocaml_av_read(value _av)
{
  CAMLparam1(_av);
  CAMLlocal1(ans);
  av_t * av = Av_val(_av);
  AVPacket * packet = &av->packet;
  stream_t ** streams = av->streams;
  stream_t * stream = NULL;
  int nb_streams = av->nb_streams;
  int frame_tag = -1;
  
  for(; frame_tag < 0;) {
    if( ! av->end_of_file) {
  
      if(packet->size <= 0) {
	// read frames from the file
	caml_release_runtime_system();
	int ret = av_read_frame(av->format, packet);
	caml_acquire_runtime_system();

	if(ret < 0) {
	  packet->data = NULL;
	  packet->size = 0;
	  av->end_of_file = 1;
	  continue;
	}
      }
  
      if(packet->stream_index >= nb_streams ||
	 (stream = streams[packet->stream_index]) == NULL) {
  
	if(av->selected_streams) {
	  av_packet_unref(packet);
	  continue;
	}
	else {
	  stream = open_stream_index(av, packet->stream_index);
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
  
    frame_tag = decode_packet(av, stream);
  }
  
  ans = caml_alloc_small(2, frame_tag);

  Field(ans, 0) = Val_int(stream->index);

  if(frame_tag == SUBTITLE_FRAME_TAG) {
    Field(ans, 1) = _av;
  }
  else {
    Field(ans, 1) = stream->frame;
  }
  
  CAMLreturn(ans);
}

CAMLprim value ocaml_av_video_to_string(value _av)
{
  CAMLparam1(_av);
  CAMLlocal1(ans);

  av_t * av = Av_val(_av);
  stream_t * stream = av->audio_stream;
  AVFrame *frame = Frame_val(stream->frame);
  size_t len = frame->nb_samples * av_get_bytes_per_sample(frame->format);

  ans = caml_alloc_string(len);
  memcpy(String_val(ans), frame->extended_data[0], len);

  CAMLreturn(ans);
}


CAMLprim value ocaml_av_subtitle_to_string(value _av)
{
  CAMLparam1(_av);
  CAMLlocal1(ans);

  av_t * av = Av_val(_av);
  unsigned num_rects = av->subtitle_stream->subtitle->num_rects;
  AVSubtitleRect **rects = av->subtitle_stream->subtitle->rects;
  size_t len = 0;
  
  for(unsigned i = 0; i < num_rects; i++) len += strlen(rects[i]->text);
    
  ans = caml_alloc_string(len + 1);
  char * dest = String_val(ans);

  for(unsigned i = 0; i < num_rects; i++) strcat(dest, rects[i]->text);

  CAMLreturn(ans);
}

