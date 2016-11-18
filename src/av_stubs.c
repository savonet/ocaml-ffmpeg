#include <pthread.h>

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

#include "avutil_stubs.h"
#include "swresample_stubs.h"

#define ERROR_MSG_SIZE 256
static char error_msg[ERROR_MSG_SIZE + 1];

static int registering_done = 0;
static pthread_mutex_t registering_mutex = PTHREAD_MUTEX_INITIALIZER;

/**** Context ****/

typedef struct {
  int index;
  AVCodecContext *codec;
  int got_frame;
  AVSubtitle *subtitle;
} stream_t;

typedef struct {
  AVFormatContext *format;
  AVPacket packet;
  value frame;
  int end_of_file;
  int selected_streams;
  stream_t ** streams;
  stream_t * audio_stream;
  stream_t * video_stream;
  stream_t * subtitle_stream;
} av_t;

typedef enum {
  undefined,
  audio_frame,
  video_frame,
  subtitle_frame,
  end_of_file,
  error
} frame_kind;
  
#define AUDIO_TAG 0
#define VIDEO_TAG 0
#define SUBTITLE_TAG 0
#define AUDIO_MEDIA_TAG 0
#define VIDEO_MEDIA_TAG 1
#define SUBTITLE_MEDIA_TAG 2
#define END_OF_FILE_TAG 0

#define Av_val(v) (*(av_t**)Data_custom_val(v))

static void free_stream(stream_t * stream)
{
  if( ! stream) return;
  
  if(stream->codec) avcodec_free_context(&stream->codec);

  if(stream->subtitle) {
    avsubtitle_free(stream->subtitle);
    free(stream->subtitle);
  }

  free(stream);
}

static void close_av(av_t * av)
{
  if( ! av) return;

  if(av->format) {

    if(av->streams) {
      for(int i = 0; i < av->format->nb_streams; i++) {
	if(av->streams[i]) free_stream(av->streams[i]);
      }
      free(av->streams);
      av->streams = NULL;
    }
    avformat_close_input(&av->format);

    av->audio_stream = NULL;
    av->video_stream = NULL;
    av->subtitle_stream = NULL;

    av_packet_unref(&av->packet);
  }
}

static void free_av(av_t * av)
{
  if( ! av) return;

  close_av(av);
  
  if(av->frame) caml_remove_generational_global_root(&av->frame);

  free(av);
}

static void finalize_av(value v)
{
  free_av(Av_val(v));
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

CAMLprim value ocaml_av_open_input(value _filename)
{
  CAMLparam1(_filename);
  CAMLlocal1(ans);
  char * error = NULL;
  int ret;

  // open input file, and allocate format context
  size_t filename_length = caml_string_length(_filename) + 1;
  char *filename = (char*)malloc(filename_length);

  if ( ! filename) {
    Raise(EXN_FAILURE, "Failed to allocate filename");
  }
  memcpy(filename, String_val(_filename), filename_length);

  caml_release_runtime_system();

  if( ! registering_done) {
    pthread_mutex_lock(&registering_mutex);

    if( ! registering_done) {
      av_register_all();
      registering_done = 1;
    }
    pthread_mutex_unlock(&registering_mutex);
  }

  av_t *av = (av_t*)calloc(1, sizeof(av_t));

  if ( ! av) {
    error = "Failed to allocate input context";
  }
  else {
    ret = avformat_open_input(&av->format, filename, NULL, NULL);
  
    if (ret < 0 || ! av->format) {
      snprintf(error_msg, ERROR_MSG_SIZE, "Failed to open file %s", filename);
      error = error_msg;
    }
  }

  free(filename);

  // retrieve stream information
  if( ! error) {
    ret = avformat_find_stream_info(av->format, NULL);

    if (ret < 0) {
      error = "Stream information not found";
    }
  }

  caml_acquire_runtime_system();

  if(error) {
    free_av(av);
    Raise(EXN_FAILURE, error);
  }

  ans = caml_alloc_custom(&av_ops, sizeof(av_t*), 0, 1);
  Av_val(ans) = av;

  CAMLreturn(ans);
}

CAMLprim value ocaml_av_close_input(value _av)
{
  CAMLparam1(_av);

  close_av(Av_val(_av));

  CAMLreturn(Val_unit);
}

CAMLprim value ocaml_av_get_metadata(value _av) {
  CAMLparam1(_av);
  CAMLlocal3(pair, cons, list);

  av_t * av = Av_val(_av);
  AVDictionary * metadata = av->format->metadata;
  AVDictionaryEntry *tag = NULL;

  list = Val_emptylist;

  while ((tag = av_dict_get(metadata, "", tag, AV_DICT_IGNORE_SUFFIX))) {

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

CAMLprim value ocaml_av_get_audio_stream_index(value _av)
{
  CAMLparam1(_av);
  av_t * av = Av_val(_av);
  int index = -1;
  
  if(av->audio_stream) {
    index = av->audio_stream->index;
  }
  else {
    index = av_find_best_stream(av->format, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
  }

  CAMLreturn(Val_int(index));
}

CAMLprim value ocaml_av_get_video_stream_index(value _av)
{
  CAMLparam1(_av);
  av_t * av = Av_val(_av);
  int index = -1;
  
  if(av->video_stream) {
    index = av->video_stream->index;
  }
  else {
    index = av_find_best_stream(av->format, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
  }

  CAMLreturn(Val_int(index));
}


CAMLprim value ocaml_av_get_duration(value _av, value _stream_index, value _time_format)
{
  CAMLparam2(_av, _time_format);
  CAMLlocal1(ans);
  av_t * av = Av_val(_av);
  int index = Int_val(_stream_index);
  int time_format = Time_format_val(_time_format);
  
  if(index >= av->format->nb_streams) {
    snprintf(error_msg, ERROR_MSG_SIZE, "Failed to get stream %d duration : index out of bounds", index);
    Raise(EXN_FAILURE, error_msg);
  }

  int64_t duration = av->format->duration;
  int64_t num = 1;
  int64_t den = AV_TIME_BASE;
  
  if(index >= 0) {
    duration = av->format->streams[index]->duration;
    num = (int64_t)av->format->streams[index]->time_base.num;
    den = (int64_t)av->format->streams[index]->time_base.den;
  }

  int64_t second_fractions = second_fractions_of_time_format(time_format);

  ans = caml_copy_int64((duration * second_fractions * num) / den);

  CAMLreturn(ans);
}


static void allocate_read_context(av_t *av)
{
  if ( ! av->format) {
    Raise(EXN_FAILURE, "Failed to read closed input");
  }

  // Allocate streams context array
  av->streams = (stream_t**)calloc(av->format->nb_streams, sizeof(stream_t*));

  if ( ! av->streams) {
    Raise(EXN_FAILURE, "Failed to allocate streams array");
  }

  // initialize packet, set data to NULL, let the demuxer fill it
  av_init_packet(&av->packet);
  av->packet.data = NULL;
  av->packet.size = 0;

  // Allocate the frame
  AVFrame * frame = av_frame_alloc();

  if ( ! frame) {
    Raise(EXN_FAILURE, "Failed to allocate frame");
  }

  value_of_frame(frame, &av->frame);
  caml_register_generational_global_root(&av->frame);
}

static stream_t * open_stream_index(av_t *av, int index)
{
  if(index < 0 || index >= av->format->nb_streams) {
    snprintf(error_msg, ERROR_MSG_SIZE, "Failed to open stream %d : index out of bounds", index);
    Raise(EXN_FAILURE, error_msg);
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

  // Open the decoder
  ret = avcodec_open2(codec, dec, NULL);

  if (ret < 0) {
    free_stream(stream);
    snprintf(error_msg, ERROR_MSG_SIZE, "Failed to open stream %d %s codec : %s", index, av_get_media_type_string(type), av_err2str(ret));
    Raise(EXN_FAILURE, error_msg);
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

  if( ! av->streams) {
    allocate_read_context(av);
  }

  av->streams[index] = stream;

  return stream;
}

static stream_t * open_best_stream(av_t *av, enum AVMediaType type)
{
  if ( ! av->format) {
    snprintf(error_msg, ERROR_MSG_SIZE, "Failed to find %s stream : input closed", av_get_media_type_string(type));
    Raise(EXN_FAILURE, error_msg);
  }

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


CAMLprim value ocaml_av_get_channel_layout(value _av) {
  CAMLparam1(_av);
  av_t * av = Av_val(_av);

  if( ! av->audio_stream) open_audio_stream(av);

  CAMLreturn(Val_channelLayout(av->audio_stream->codec->channel_layout));
}

CAMLprim value ocaml_av_get_nb_channels(value _av) {
  CAMLparam1(_av);
  av_t * av = Av_val(_av);

  if( ! av->audio_stream) open_audio_stream(av);

  CAMLreturn(Val_int(av_get_channel_layout_nb_channels(av->audio_stream->codec->channel_layout)));
}

CAMLprim value ocaml_av_get_sample_rate(value _av) {
  CAMLparam1(_av);
  av_t * av = Av_val(_av);

  if( ! av->audio_stream) open_audio_stream(av);

  CAMLreturn(Val_int(av->audio_stream->codec->sample_rate));
}

CAMLprim value ocaml_av_get_sample_format(value _av) {
  CAMLparam1(_av);
  av_t * av = Av_val(_av);

  if( ! av->audio_stream) open_audio_stream(av);

  CAMLreturn(Val_sampleFormat(av->audio_stream->codec->sample_fmt));
}

static frame_kind decode_packet(av_t * av, stream_t * stream, AVFrame * frame)
{
  AVPacket * packet = &av->packet;
  AVCodecContext * dec = stream->codec;
  frame_kind frame_kind = undefined;
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
      av->audio_stream = stream;
      frame_kind = audio_frame;
    }
    else {
      av->video_stream = stream;
      frame_kind = video_frame;
    }
  }
  else if(dec->codec_type == AVMEDIA_TYPE_SUBTITLE) {
  
    ret = avcodec_decode_subtitle2(dec, stream->subtitle, &stream->got_frame, packet);
  
    if (ret >= 0) packet->size = 0;
  
    av->subtitle_stream = stream;
    frame_kind = subtitle_frame;
  }
  
  if(packet->size <= 0) {
    av_packet_unref(packet);
  }
  
  if(ret >= 0) {
    stream->got_frame = 1;
  }
  else if (ret == AVERROR_EOF) {
    stream->got_frame = 0;
    frame_kind = end_of_file;
  }
  else if (ret == AVERROR(EAGAIN)) {
    frame_kind = undefined;
  }
  else {
    snprintf(error_msg, ERROR_MSG_SIZE, "Error decoding %s frame (%s)", av_get_media_type_string(dec->codec_type), av_err2str(ret));
    frame_kind = error;
  }
    
  return frame_kind;
}

static frame_kind read_stream_frame(av_t * av, stream_t * stream)
{
  AVPacket * packet = &av->packet;
  int stream_index = stream->index;
  AVFrame *frame = Frame_val(av->frame);
  frame_kind frame_kind = undefined;

  caml_release_runtime_system();

  for(; frame_kind == undefined;) {
    if( ! av->end_of_file) {

      if(packet->size <= 0) {
	// read frames from the file
	int ret = av_read_frame(av->format, packet);

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
    frame_kind = decode_packet(av, stream, frame);
  }
  
  caml_acquire_runtime_system();

  if(frame_kind == error) {
    Raise(EXN_FAILURE, error_msg);
  }
  return frame_kind;
}

CAMLprim value ocaml_av_read_audio(value _av)
{
  CAMLparam1(_av);
  CAMLlocal1(ans);
  av_t * av = Av_val(_av);
  
  if( ! av->audio_stream) open_audio_stream(av);
  
  frame_kind frame_kind = read_stream_frame(av, av->audio_stream);

  if(frame_kind == end_of_file) {
    ans = Val_int(END_OF_FILE_TAG);
  }
  else {
    ans = caml_alloc_small(1, AUDIO_TAG);
    Field(ans, 0) = av->frame;
  }

  CAMLreturn(ans);
}

CAMLprim value ocaml_av_read_video(value _av)
{
  CAMLparam1(_av);
  CAMLlocal1(ans);
  av_t * av = Av_val(_av);

  if( ! av->video_stream) open_video_stream(av);

  frame_kind frame_kind = read_stream_frame(av, av->video_stream);

  if(frame_kind == end_of_file) {
    ans = Val_int(END_OF_FILE_TAG);
  }
  else {
    ans = caml_alloc_small(1, VIDEO_TAG);
    Field(ans, 0) = av->frame;
  }
  CAMLreturn(ans);
}

CAMLprim value ocaml_av_read_subtitle(value _av)
{
  CAMLparam1(_av);
  CAMLlocal1(ans);
  av_t * av = Av_val(_av);

  if( ! av->subtitle_stream) open_subtitle_stream(av);

  frame_kind frame_kind = read_stream_frame(av, av->subtitle_stream);

  if(frame_kind == end_of_file) {
    ans = Val_int(END_OF_FILE_TAG);
  }
  else {
    ans = caml_alloc_small(1, SUBTITLE_TAG);
    Field(ans, 0) = _av;
  }
  CAMLreturn(ans);
}

CAMLprim value ocaml_av_read(value _av)
{
  CAMLparam1(_av);
  CAMLlocal1(ans);
  av_t * av = Av_val(_av);

  if( ! av->streams) {
    allocate_read_context(av);
  }

  AVPacket * packet = &av->packet;
  AVFrame *frame = Frame_val(av->frame);
  stream_t ** streams = av->streams;
  int nb_streams = av->format->nb_streams;
  stream_t * stream = NULL;
  frame_kind frame_kind = undefined;

  caml_release_runtime_system();

  for(; frame_kind == undefined;) {
    if( ! av->end_of_file) {
  
      if(packet->size <= 0) {
	// read frames from the file
	int ret = av_read_frame(av->format, packet);

	if(ret < 0) {
	  packet->data = NULL;
	  packet->size = 0;
	  av->end_of_file = 1;
	  continue;
	}
      }
  
      if((stream = streams[packet->stream_index]) == NULL) {
  
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
      // If the end of file is reached, iteration on the streams to find one to flush
      int i = 0;
      for(; i < nb_streams; i++) {
	if((stream = streams[i]) && stream->got_frame) break;
      }
  
      if(i == nb_streams) {
	frame_kind = end_of_file;
	break;
      }
    }
  
    frame_kind = decode_packet(av, stream, frame);
  }

  caml_acquire_runtime_system();
  
  if(frame_kind == end_of_file) {
    ans = Val_int(END_OF_FILE_TAG);
  }
  else if(frame_kind == error) {
    Raise(EXN_FAILURE, error_msg);
  }
  else {
    int media_tag = AUDIO_MEDIA_TAG;

    if(frame_kind == video_frame) {
      media_tag = VIDEO_MEDIA_TAG;
    }
    else if(frame_kind == subtitle_frame) {
      media_tag = SUBTITLE_MEDIA_TAG;
    }
    
    ans = caml_alloc_small(2, media_tag);

    Field(ans, 0) = Val_int(stream->index);

    if(frame_kind == subtitle_frame) {
      Field(ans, 1) = _av;
    }
    else {
      Field(ans, 1) = av->frame;
    }
  }
  CAMLreturn(ans);
}


static const int seek_flags[] = {AVSEEK_FLAG_BACKWARD, AVSEEK_FLAG_BYTE, AVSEEK_FLAG_ANY, AVSEEK_FLAG_FRAME};

static int seek_flags_val(value v)
{
  return seek_flags[Int_val(v)];
}

CAMLprim value ocaml_av_seek_frame(value _av, value _stream_index, value _time_format, value _timestamp, value _flags)
{
  CAMLparam4(_av, _time_format, _timestamp, _flags);

  av_t * av = Av_val(_av);
  int index = Int_val(_stream_index);
  int time_format = Time_format_val(_time_format);
  int64_t timestamp = Int64_val(_timestamp);

  if( ! av->format || index >= av->format->nb_streams) {
    snprintf(error_msg, ERROR_MSG_SIZE, "Failed to seek stream %d : index out of bounds", index);
    Raise(EXN_FAILURE, error_msg);
  }
  
  int64_t num = AV_TIME_BASE;
  int64_t den = 1;
  
  if(index >= 0) {
    num = (int64_t)av->format->streams[index]->time_base.den;
    den = (int64_t)av->format->streams[index]->time_base.num;
  }

  int64_t second_fractions = second_fractions_of_time_format(time_format);

  timestamp = (timestamp * num) / (den * second_fractions);

  int flags = 0;
  for (int i = 0; i < Wosize_val(_flags); i++)
    flags |= seek_flags_val(Field(_flags, i));

  caml_release_runtime_system();
  int ret = av_seek_frame(av->format, index, timestamp, flags);
  caml_acquire_runtime_system();

  if (ret < 0) Raise(EXN_FAILURE, "Av seek frame failed");

  CAMLreturn(Val_unit);
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
