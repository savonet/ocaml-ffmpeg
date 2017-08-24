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
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include "libavutil/audio_fifo.h"

#include "avutil_stubs.h"
#include "avcodec_stubs.h"
#include "swresample_stubs.h"

/**** Context ****/

typedef struct {
  int index;
  AVCodecContext *codec_context;
  AVSubtitle *subtitle;

  // input
  int got_frame;

  // output
  AVAudioFifo *audio_fifo;
  AVFrame *frame;
  int64_t pts;
} stream_t;

typedef struct {
  AVFormatContext *format_context;
  AVPacket packet;

  // input
  value frame;
  int end_of_file;
  int selected_streams;
  stream_t ** streams;
  stream_t * audio_stream;
  stream_t * video_stream;
  stream_t * subtitle_stream;

  // output
  int header_written;
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
  
  if(stream->codec_context) avcodec_free_context(&stream->codec_context);

  if(stream->subtitle) {
    avsubtitle_free(stream->subtitle);
    free(stream->subtitle);
  }

  if(stream->frame) {
    av_frame_free(&stream->frame);
  }

  if(stream->audio_fifo) {
    av_audio_fifo_free(stream->audio_fifo);
  }

  free(stream);
}

static void close_av(av_t * av)
{
  if( ! av) return;

  if(av->format_context) {

    if(av->streams) {
      for(int i = 0; i < av->format_context->nb_streams; i++) {
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

static av_t * open_input(char * filename)
{
  av_t *av = (av_t*)calloc(1, sizeof(av_t));
  if ( ! av) Fail("Failed to allocate input context");

  int ret = avformat_open_input(&av->format_context, filename, NULL, NULL);
  
  if (ret < 0 || ! av->format_context) {
    free(av);
    Fail("Failed to open file %s", filename);
  }

  // retrieve stream information
  ret = avformat_find_stream_info(av->format_context, NULL);

  if (ret < 0) {
    free(av);
    Fail("Stream information not found");
  }
  return av;
}

CAMLprim value ocaml_av_open_input(value _filename)
{
  CAMLparam1(_filename);
  CAMLlocal1(ans);

  // open input file, and allocate format context
  char * filename = caml_strdup(String_val(_filename));

  av_register_all();

  caml_release_runtime_system();

  av_t *av = open_input(filename);

  caml_stat_free(filename);

  caml_acquire_runtime_system();
  if( ! av) Raise(EXN_FAILURE, ocaml_av_error_msg);

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
  AVDictionary * metadata = av->format_context->metadata;
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
    index = av_find_best_stream(av->format_context, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
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
    index = av_find_best_stream(av->format_context, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
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
  
  if(index >= av->format_context->nb_streams) Raise(EXN_FAILURE, "Failed to get stream %d duration : index out of bounds", index);

  int64_t duration = av->format_context->duration;
  int64_t num = 1;
  int64_t den = AV_TIME_BASE;
  
  if(index >= 0) {
    duration = av->format_context->streams[index]->duration;
    num = (int64_t)av->format_context->streams[index]->time_base.num;
    den = (int64_t)av->format_context->streams[index]->time_base.den;
  }

  int64_t second_fractions = second_fractions_of_time_format(time_format);

  ans = caml_copy_int64((duration * second_fractions * num) / den);

  CAMLreturn(ans);
}


static stream_t** allocate_read_context(av_t *av)
{
  if( ! av->format_context) Fail("Failed to read closed input");
  
  // Allocate streams context array
  av->streams = (stream_t**)calloc(av->format_context->nb_streams, sizeof(stream_t*));
  if( ! av->streams) Fail("Failed to allocate streams array");

  // initialize packet, set data to NULL, let the demuxer fill it
  av_init_packet(&av->packet);
  av->packet.data = NULL;
  av->packet.size = 0;

  // Allocate the frame
  AVFrame * frame = av_frame_alloc();
  if( ! frame) Fail("Failed to allocate frame");

  value_of_frame(frame, &av->frame);
  caml_register_generational_global_root(&av->frame);

  return av->streams;
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

  if(type == AVMEDIA_TYPE_SUBTITLE) {
    stream->subtitle = (AVSubtitle*)calloc(1, sizeof(AVSubtitle));
    if( ! stream->subtitle) Fail("Failed to allocate stream subtitle context");
  }

  return stream;
}

static stream_t * open_stream_index(av_t *av, int index)
{
  if(index < 0 || index >= av->format_context->nb_streams) Fail("Failed to open stream %d : index out of bounds", index);
  
  if( ! av->streams && ! allocate_read_context(av)) return NULL;

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

static stream_t * open_best_stream(av_t *av, enum AVMediaType type)
{
  if( ! av->format_context) Fail("Failed to find stream : input closed");

  int index = av_find_best_stream(av->format_context, type, -1, -1, NULL, 0);

  if(index < 0) Fail("Failed to find %s stream", av_get_media_type_string(type));

  return open_stream_index(av, index);
}

static stream_t * open_audio_stream(av_t * av)
{
  return (av->audio_stream = open_best_stream(av, AVMEDIA_TYPE_AUDIO));
}

static stream_t * open_video_stream(av_t * av)
{
  return (av->video_stream = open_best_stream(av, AVMEDIA_TYPE_VIDEO));
}

static stream_t * open_subtitle_stream(av_t * av)
{
  return (av->subtitle_stream = open_best_stream(av, AVMEDIA_TYPE_SUBTITLE));
}


CAMLprim value ocaml_av_get_channel_layout(value _av) {
  CAMLparam1(_av);
  av_t * av = Av_val(_av);

  if( ! av->audio_stream && ! open_audio_stream(av)) Raise(EXN_FAILURE, ocaml_av_error_msg);

  CAMLreturn(Val_channelLayout(av->audio_stream->codec_context->channel_layout));
}

CAMLprim value ocaml_av_get_nb_channels(value _av) {
  CAMLparam1(_av);
  av_t * av = Av_val(_av);

  if( ! av->audio_stream && ! open_audio_stream(av)) Raise(EXN_FAILURE, ocaml_av_error_msg);

  CAMLreturn(Val_int(av_get_channel_layout_nb_channels(av->audio_stream->codec_context->channel_layout)));
}

CAMLprim value ocaml_av_get_sample_rate(value _av) {
  CAMLparam1(_av);
  av_t * av = Av_val(_av);

  if( ! av->audio_stream && ! open_audio_stream(av)) Raise(EXN_FAILURE, ocaml_av_error_msg);

  CAMLreturn(Val_int(av->audio_stream->codec_context->sample_rate));
}

CAMLprim value ocaml_av_get_sample_format(value _av) {
  CAMLparam1(_av);
  av_t * av = Av_val(_av);

  if( ! av->audio_stream && ! open_audio_stream(av)) Raise(EXN_FAILURE, ocaml_av_error_msg);

  CAMLreturn(Val_sampleFormat(av->audio_stream->codec_context->sample_fmt));
}

static frame_kind decode_packet(av_t * av, stream_t * stream, AVFrame * frame)
{
  AVPacket * packet = &av->packet;
  AVCodecContext * dec = stream->codec_context;
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
    Log("Failed to decode %s frame : %s", av_get_media_type_string(dec->codec_type), av_err2str(ret));
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
        int ret = av_read_frame(av->format_context, packet);

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
  if(frame_kind == error) Raise(EXN_FAILURE, ocaml_av_error_msg);

  return frame_kind;
}

CAMLprim value ocaml_av_read_audio(value _av)
{
  CAMLparam1(_av);
  CAMLlocal1(ans);
  av_t * av = Av_val(_av);
  
  if( ! av->audio_stream && ! open_audio_stream(av)) Raise(EXN_FAILURE, ocaml_av_error_msg);
  
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

  if( ! av->video_stream && ! open_video_stream(av)) Raise(EXN_FAILURE, ocaml_av_error_msg);

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

  if( ! av->subtitle_stream && ! open_subtitle_stream(av)) Raise(EXN_FAILURE, ocaml_av_error_msg);

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

  if(! av->streams && ! allocate_read_context(av)) Raise(EXN_FAILURE, ocaml_av_error_msg);

  AVPacket * packet = &av->packet;
  AVFrame *frame = Frame_val(av->frame);
  stream_t ** streams = av->streams;
  int nb_streams = av->format_context->nb_streams;
  stream_t * stream = NULL;
  frame_kind frame_kind = undefined;

  caml_release_runtime_system();

  for(; frame_kind == undefined;) {
    if( ! av->end_of_file) {
  
      if(packet->size <= 0) {
        // read frames from the file
        int ret = av_read_frame(av->format_context, packet);

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
          if(NULL == (stream = open_stream_index(av, packet->stream_index))) {
            frame_kind = error;
            break;
          }
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
  if(frame_kind == error) Raise(EXN_FAILURE, ocaml_av_error_msg);
  
  if(frame_kind == end_of_file) {
    ans = Val_int(END_OF_FILE_TAG);
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

  if( ! av->format_context || index >= av->format_context->nb_streams) Raise(EXN_FAILURE, "Failed to seek stream %d : index out of bounds", index);
  
  int64_t num = AV_TIME_BASE;
  int64_t den = 1;
  
  if(index >= 0) {
    num = (int64_t)av->format_context->streams[index]->time_base.den;
    den = (int64_t)av->format_context->streams[index]->time_base.num;
  }

  int64_t second_fractions = second_fractions_of_time_format(time_format);

  timestamp = (timestamp * num) / (den * second_fractions);

  int flags = 0;
  for (int i = 0; i < Wosize_val(_flags); i++)
    flags |= seek_flags_val(Field(_flags, i));

  caml_release_runtime_system();

  int ret = av_seek_frame(av->format_context, index, timestamp, flags);

  caml_acquire_runtime_system();

  if(ret < 0) Raise(EXN_FAILURE, "Av seek frame failed : %s", av_err2str(ret));

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

static av_t * open_output(const char * filename)
{
  av_t *av = (av_t*)calloc(1, sizeof(av_t));
  if ( ! av) Fail("Failed to allocate %s output context", filename);

  avformat_alloc_output_context2(&av->format_context, NULL, NULL, filename);
  
  if ( ! av->format_context) {
    free_av(av);
    Fail("Failed to allocate %s format context", filename);
  }

  // open the output file, if needed
  if( ! (av->format_context->oformat->flags & AVFMT_NOFILE)) {
    int ret = avio_open(&av->format_context->pb, filename, AVIO_FLAG_WRITE);
    if (ret < 0) {
      free_av(av);
      Fail("Failed to open '%s': %s\n", filename, av_err2str(ret));
    }
  }
  return av;
}

CAMLprim value ocaml_av_open_output(value _filename)
{
  CAMLparam1(_filename);
  CAMLlocal1(ans);

  // open output file, and allocate format context
  char * filename = caml_strdup(String_val(_filename));

  av_register_all();

  caml_release_runtime_system();

  av_t *av = open_output(filename);
  
  caml_stat_free(filename);

  caml_acquire_runtime_system();

  if( ! av) Raise(EXN_FAILURE, ocaml_av_error_msg);

  ans = caml_alloc_custom(&av_ops, sizeof(av_t*), 0, 1);
  Av_val(ans) = av;

  CAMLreturn(ans);
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
  if( ! avstream) Fail("Failed allocating output stream");

  avstream->id = av->format_context->nb_streams - 1;

  // initialize packet, set data to NULL, let the muxer fill it
  av_init_packet(&av->packet);
  av->packet.data = NULL;
  av->packet.size = 0;

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

CAMLprim value ocaml_av_new_audio_stream(value _av, value _audio_codec_id, value _channel_layout, value _sample_fmt, value _bit_rate, value _sample_rate)
{
  CAMLparam4(_av, _audio_codec_id, _channel_layout, _sample_fmt);
  av_t * av = Av_val(_av);
 
  stream_t * stream = new_stream(av, AudioCodecId_val(_audio_codec_id));
  if( ! stream) Raise(EXN_FAILURE, ocaml_av_error_msg);
  
  AVCodecContext * enc_ctx = stream->codec_context;

  enc_ctx->bit_rate = Int_val(_bit_rate);
  enc_ctx->sample_rate = Int_val(_sample_rate);
  enc_ctx->channel_layout = ChannelLayout_val(_channel_layout);
  enc_ctx->channels = av_get_channel_layout_nb_channels(enc_ctx->channel_layout);
  enc_ctx->sample_fmt = SampleFormat_val(_sample_fmt);
  enc_ctx->time_base = (AVRational){1, enc_ctx->sample_rate};

  stream = init_stream_encoder(av, stream);
  if( ! stream) Raise(EXN_FAILURE, ocaml_av_error_msg);

  // Allocate the frame
  stream->frame = av_frame_alloc();
  if( ! stream->frame) Raise(EXN_FAILURE, "Failed to allocate frame");

  stream->frame->nb_samples     = enc_ctx->frame_size;
  stream->frame->channel_layout = enc_ctx->channel_layout;
  stream->frame->format         = enc_ctx->sample_fmt;
  stream->frame->sample_rate    = enc_ctx->sample_rate;

  int ret = av_frame_get_buffer(stream->frame, 0);
  if (ret < 0) Raise(EXN_FAILURE, "Failed to allocate output frame samples : %s)", av_err2str(ret));
  
  // Create the FIFO buffer based on the specified output sample format.
  stream->audio_fifo = av_audio_fifo_alloc(enc_ctx->sample_fmt, enc_ctx->channels, 1);
  if( ! stream->audio_fifo) Raise(EXN_FAILURE, "Failed to allocate audio FIFO");

  CAMLreturn(Val_int(stream->index));
}

CAMLprim value ocaml_av_new_audio_stream_byte(value *argv, int argn)
{
  return ocaml_av_new_audio_stream(argv[0], argv[1], argv[2], argv[3], argv[4], argv[5]);
}

CAMLprim value ocaml_av_new_video_stream(value _av, value _video_codec_id, value _width, value _height, value _pix_fmt, value _bit_rate, value _frame_rate)
{
  CAMLparam3(_av, _video_codec_id, _pix_fmt);
  av_t * av = Av_val(_av);
  
  stream_t * stream = new_stream(av, VideoCodecId_val(_video_codec_id));
  if( ! stream) Raise(EXN_FAILURE, ocaml_av_error_msg);
  
  AVCodecContext * enc_ctx = stream->codec_context;

  enc_ctx->bit_rate = Int_val(_bit_rate);
  enc_ctx->width = Int_val(_width);
  enc_ctx->height = Int_val(_height);
  enc_ctx->pix_fmt = PixelFormat_val(_pix_fmt);
  enc_ctx->time_base = (AVRational){1, Int_val(_frame_rate)};
  enc_ctx->framerate = (AVRational){Int_val(_frame_rate), 1};
 
  stream = init_stream_encoder(av, stream);
  if( ! stream) Raise(EXN_FAILURE, ocaml_av_error_msg);

  CAMLreturn(Val_int(stream->index));
}

CAMLprim value ocaml_av_new_video_stream_byte(value *argv, int argn)
{
  return ocaml_av_new_video_stream(argv[0], argv[1], argv[2], argv[3], argv[4], argv[5], argv[6]);
}

static int write_frame(av_t * av, int stream_index, AVCodecContext * enc_ctx, AVFrame * frame)
{
  if( ! av->header_written) {
    // write output file header
    int ret = avformat_write_header(av->format_context, NULL);
    if(ret < 0) {
      Log("Failed to write header : %s", av_err2str(ret));
      return ret;
    }

    av->header_written = 1;
  }

  av->packet.stream_index = stream_index;

  // send the frame for encoding
  int ret = avcodec_send_frame(enc_ctx, frame);
  if(ret < 0) Log("Failed to send the frame to the encoder : %s", av_err2str(ret));

  // read all the available output packets (in general there may be any number of them
  while (ret >= 0) {
    ret = avcodec_receive_packet(enc_ctx, &av->packet);
 
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;

    if (ret < 0) {
      Log("Failed to encode frame : %s", av_err2str(ret));
      break;
    }

    ret = av_interleaved_write_frame(av->format_context, &av->packet);
    if(ret < 0) Log("Failed to write frame : %s", av_err2str(ret));

    av_packet_unref(&av->packet);
  }
  return ret;
}

static AVPacket * write_audio_frame(av_t * av, int stream_index, AVFrame * frame)
{
  int ret;
  stream_t * stream = av->streams[stream_index];
  AVAudioFifo *fifo = stream->audio_fifo;
  AVFrame * output_frame = stream->frame;
  
  int fifo_size = av_audio_fifo_size(fifo);
  int frame_size = fifo_size;

  if( ! stream->codec_context) Fail("Failed to write audio frame with no encoder");

  if(frame != NULL) {
    frame_size = stream->codec_context->frame_size;
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

    ret = write_frame(av, stream_index, stream->codec_context, output_frame);
    
    if (ret == AVERROR_EOF) return &av->packet;
    if (ret < 0 && ret != AVERROR(EAGAIN)) return NULL;
  }
  return &av->packet;
}

static AVPacket * write_video_frame(av_t * av, int stream_index, AVFrame * frame)
{
  int ret;
  stream_t * stream = av->streams[stream_index];
  
  if( ! stream->codec_context) Fail("Failed to write video frame with no encoder");

  if(frame) {
    frame->pts = stream->pts++;
  }
    
  ret = write_frame(av, stream_index, stream->codec_context, frame);
    
  if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) return NULL;

  return &av->packet;
}

CAMLprim value ocaml_av_write_audio_frame(value _av, value _stream_index, value _frame) {
  CAMLparam2(_av, _frame);
  av_t * av = Av_val(_av);
  AVFrame * frame = Frame_val(_frame);

  caml_release_runtime_system();
  AVPacket * packet = write_audio_frame(av, Int_val(_stream_index), frame);
  caml_acquire_runtime_system();

  if( ! packet) Raise(EXN_FAILURE, ocaml_av_error_msg);
  CAMLreturn(Val_unit);
}

CAMLprim value ocaml_av_write_video_frame(value _av, value _stream_index, value _frame) {
  CAMLparam2(_av, _frame);
  av_t * av = Av_val(_av);
  AVFrame * frame = Frame_val(_frame);

  caml_release_runtime_system();
  AVPacket * packet = write_video_frame(av, Int_val(_stream_index), frame);
  caml_acquire_runtime_system();

  if( ! packet) Raise(EXN_FAILURE, ocaml_av_error_msg);
  CAMLreturn(Val_unit);
}

CAMLprim value ocaml_av_close_output(value _av)
{
  CAMLparam1(_av);
  av_t * av = Av_val(_av);

  // flush encoders
  AVPacket * packet = &av->packet;
  caml_release_runtime_system();

  for(int i = 0; i < av->format_context->nb_streams && packet; i++) {

    AVCodecContext * enc_ctx = av->streams[i]->codec_context;

    if( ! enc_ctx) continue;
  
    if(enc_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
      packet = write_audio_frame(av, i, NULL);
    }
    else {
      packet = write_video_frame(av, i, NULL);
    }
  }

  caml_acquire_runtime_system();

  if( ! packet) Raise(EXN_FAILURE, ocaml_av_error_msg);

  // write the trailer
  av_write_trailer(av->format_context);

  close_av(av);

  CAMLreturn(Val_unit);
}
