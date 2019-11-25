#include <assert.h>
#include <pthread.h>

#include <caml/mlvalues.h>
#include <caml/memory.h>
#include <caml/alloc.h>
#include <caml/fail.h>
#include <caml/callback.h>
#include <caml/custom.h>
#include <caml/bigarray.h>
#include <caml/threads.h>

#include <libavutil/pixfmt.h>
#include <libavutil/pixdesc.h>
#include <libavutil/avstring.h>

#include "avutil_stubs.h"
#include "pixel_format_stubs.h"
#include "channel_layout_stubs.h"
#include "sample_format_stubs.h"

char ocaml_av_exn_msg[ERROR_MSG_SIZE + 1];

void ocaml_avutil_raise_error(int err) {
  int _err = 0;

  switch (err) {
    case AVERROR_BSF_NOT_FOUND:
      _err = PVV_Bsf_not_found;
      break;
    case AVERROR_DECODER_NOT_FOUND:
      _err = PVV_Decoder_not_found;
      break;
    case AVERROR_DEMUXER_NOT_FOUND:
      _err = PVV_Demuxer_not_found;
      break;
    case AVERROR_ENCODER_NOT_FOUND:
      _err = PVV_Encoder_not_found;
      break;
    case AVERROR_EOF:
      _err = PVV_Eof;
      break;
    case AVERROR_EXIT:
      _err = PVV_Exit;
      break;
    case AVERROR_FILTER_NOT_FOUND:
      _err = PVV_Filter_not_found;
      break;
    case AVERROR_INVALIDDATA:
      _err = PVV_Invalid_data;
      break;
    case AVERROR_MUXER_NOT_FOUND:
      _err = PVV_Muxer_not_found;
      break;
    case AVERROR_OPTION_NOT_FOUND:
      _err = PVV_Option_not_found;
      break;
    case AVERROR_PATCHWELCOME:
      _err = PVV_Patch_welcome;
      break;
    case AVERROR_PROTOCOL_NOT_FOUND:
      _err = PVV_Protocol_not_found;
      break;
    case AVERROR_STREAM_NOT_FOUND:
      _err = PVV_Stream_not_found;
      break;
    case AVERROR_BUG:
      _err = PVV_Bug;
      break;
    case AVERROR(EAGAIN):
      _err = PVV_Eagain;
      break;
    case AVERROR_UNKNOWN:
      _err = PVV_Unknown;
      break;
    case AVERROR_EXPERIMENTAL:
      _err = PVV_Experimental;
      break;
    default:
      Fail("%s", av_err2str(err));
  }

  caml_raise_with_arg(*caml_named_value(EXN_ERROR), _err);
}

CAMLprim value ocaml_avutil_string_of_error(value error) {
  CAMLparam0();
  int err;

  switch (error) {
    case PVV_Bsf_not_found:
      err = AVERROR_BSF_NOT_FOUND;
      break;
    case PVV_Decoder_not_found:
      err = AVERROR_DECODER_NOT_FOUND;
      break;
    case PVV_Demuxer_not_found:
      err = AVERROR_DEMUXER_NOT_FOUND;
      break;
    case PVV_Encoder_not_found:
      err = AVERROR_ENCODER_NOT_FOUND;
      break;
    case PVV_Eof:
      err = AVERROR_EOF;
      break;
    case PVV_Exit:
      err = AVERROR_EXIT;
      break;
    case PVV_Filter_not_found:
      err = AVERROR_FILTER_NOT_FOUND;
      break;
    case PVV_Invalid_data:
      err = AVERROR_INVALIDDATA;
      break;
    case PVV_Muxer_not_found:
      err = AVERROR_MUXER_NOT_FOUND;
      break;
    case PVV_Option_not_found:
      err = AVERROR_OPTION_NOT_FOUND;
      break;
    case PVV_Patch_welcome:
      err = AVERROR_PATCHWELCOME;
      break;
    case PVV_Protocol_not_found:
      err = AVERROR_PROTOCOL_NOT_FOUND;
      break;
    case PVV_Stream_not_found:
      err = AVERROR_STREAM_NOT_FOUND;
      break;
    case PVV_Bug:
      err = AVERROR_BUG;
      break;
    case PVV_Eagain:
      err = AVERROR(EAGAIN);
      break;
    case PVV_Unknown:
      err = AVERROR_UNKNOWN;
      break;
    case PVV_Experimental:
      err = AVERROR_EXPERIMENTAL;
      break;
    default:
      // Failure error is a 2-uple with string as second entry.
      CAMLreturn(Field(error,1));
  }

  CAMLreturn(caml_copy_string(av_err2str(err)));
}

/***** Global initialisation *****/
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58, 9, 100)
static int lock_manager(void **mtx, enum AVLockOp op)
{
  switch(op) {
  case AV_LOCK_CREATE:
    *mtx = malloc(sizeof(pthread_mutex_t));

    if(!*mtx)
      return 1;
    return !!pthread_mutex_init(*mtx, NULL);

  case AV_LOCK_OBTAIN:
    return !!pthread_mutex_lock(*mtx);

  case AV_LOCK_RELEASE:
    return !!pthread_mutex_unlock(*mtx);

  case AV_LOCK_DESTROY:
    pthread_mutex_destroy(*mtx);
    free(*mtx);
    return 0;
  }
  return 1;
}

CAMLprim value ocaml_avutil_register_lock_manager(value unit)
{
  CAMLparam0();
  static int registering_done = 0;
  static pthread_mutex_t registering_mutex = PTHREAD_MUTEX_INITIALIZER;

  if( ! registering_done) {
    pthread_mutex_lock(&registering_mutex);

    if( ! registering_done) {

      caml_release_runtime_system();
      int ret = av_lockmgr_register(lock_manager);
      caml_acquire_runtime_system();

      if(ret >= 0) {
        registering_done = 1;
      }
      pthread_mutex_unlock(&registering_mutex);
    }
  }
  CAMLreturn(Val_int(registering_done));
}
#else
value ocaml_avutil_register_lock_manager(value unit) { return Val_true; }
#endif

static pthread_key_t ocaml_c_thread_key;
static pthread_once_t ocaml_c_thread_key_once = PTHREAD_ONCE_INIT;

static void ocaml_ffmpeg_on_thread_exit(void *key) {
  caml_c_thread_unregister();
}

static void ocaml_ffmpeg_make_key() {
  pthread_key_create(&ocaml_c_thread_key, ocaml_ffmpeg_on_thread_exit);
}

void ocaml_ffmpeg_register_thread() {
  static int initialized = 1;

  pthread_once(&ocaml_c_thread_key_once, ocaml_ffmpeg_make_key);

  if (caml_c_thread_register() && !pthread_getspecific(ocaml_c_thread_key))
    pthread_setspecific(ocaml_c_thread_key,(void*)&initialized);
}

/**** Rational ****/
void value_of_rational(const AVRational * rational, value * pvalue) {
  *pvalue = caml_alloc_tuple(2);
  Field(*pvalue, 0) = Val_int(rational->num);
  Field(*pvalue, 1) = Val_int(rational->den);
}


/**** Time format ****/
int64_t second_fractions_of_time_format(value time_format)
{
  switch(time_format) {
  case PVV_Second : return 1;
  case PVV_Millisecond : return 1000;
  case PVV_Microsecond : return 1000000;
  case PVV_Nanosecond : return 1000000000;
  default : break;
  }
  return 1;
}

/**** Logging ****/
CAMLprim value ocaml_avutil_set_log_level(value level)
{
  CAMLparam0();
  av_log_set_level(Int_val(level));
  CAMLreturn(Val_unit);
}


#define LINE_SIZE 1024

static value ocaml_log_callback = (value)NULL;

static void av_log_ocaml_callback(void* ptr, int level, const char* fmt, va_list vl)
{
  static int print_prefix = 1;
  char line[LINE_SIZE];
  value buffer;

  av_log_format_line2(ptr, level, fmt, vl, line, LINE_SIZE, &print_prefix);

  ocaml_ffmpeg_register_thread();

  caml_acquire_runtime_system();

  buffer = caml_copy_string(line);

  caml_register_generational_global_root(&buffer);

  caml_callback(ocaml_log_callback, buffer);

  caml_remove_generational_global_root(&buffer);

  caml_release_runtime_system();
}

CAMLprim value ocaml_avutil_set_log_callback(value callback)
{
  CAMLparam1(callback);
  
  if (ocaml_log_callback == (value)NULL) {
    ocaml_log_callback = callback;
    caml_register_generational_global_root(&ocaml_log_callback);
  } else {
    caml_modify_generational_global_root(&ocaml_log_callback,callback);
  }

  caml_release_runtime_system();
  av_log_set_callback(&av_log_ocaml_callback);
  caml_acquire_runtime_system();

  CAMLreturn(Val_unit);
}

CAMLprim value ocaml_avutil_clear_log_callback()
{
  CAMLparam0();

  if (ocaml_log_callback != (value)NULL) {
    caml_remove_generational_global_root(&ocaml_log_callback);
    ocaml_log_callback = (value)NULL;
  }

  caml_release_runtime_system();
  av_log_set_callback(&av_log_default_callback);
  caml_acquire_runtime_system();

  CAMLreturn(Val_unit);
}

CAMLprim value ocaml_avutil_time_base()
{
  CAMLparam0();
  CAMLlocal1(ans);

  value_of_rational(&AV_TIME_BASE_Q, &ans);

  CAMLreturn(ans);
}

/**** Channel layout ****/
CAMLprim value ocaml_avutil_get_channel_layout_description(value _channel_layout, value channels)
{
  CAMLparam1(_channel_layout);
  char buf[1024];
  uint64_t channel_layout = ChannelLayout_val(_channel_layout);

  caml_release_runtime_system();
  av_get_channel_layout_string(buf, sizeof(buf), Int_val(channels), channel_layout);
  caml_acquire_runtime_system();

  CAMLreturn(caml_copy_string(buf));
}

CAMLprim value ocaml_avutil_get_channel_layout_nb_channels(value _channel_layout)
{
  CAMLparam1(_channel_layout);
  CAMLreturn(Val_int(av_get_channel_layout_nb_channels(ChannelLayout_val(_channel_layout))));
}

CAMLprim value ocaml_avutil_get_default_channel_layout(value _nb_channels)
{
  CAMLparam0();

  caml_release_runtime_system();
  int64_t ret = av_get_default_channel_layout(Int_val(_nb_channels));
  caml_acquire_runtime_system();

  if (ret == 0) caml_raise_not_found(); 

  CAMLreturn(Val_ChannelLayout(ret));
}

CAMLprim value ocaml_avutil_get_channel_layout(value _name)
{
  CAMLparam1(_name);
  char *name = strndup(String_val(_name), caml_string_length(_name));

  if (!name) caml_raise_out_of_memory();

  caml_release_runtime_system();
  int64_t ret = av_get_channel_layout(name);
  caml_acquire_runtime_system();

  free(name);

  if (ret == 0) caml_raise_not_found();

  CAMLreturn(Val_ChannelLayout(ret));
}

CAMLprim value ocaml_avutil_get_channel_layout_id(value _channel_layout)
{
  CAMLparam1(_channel_layout);
  CAMLreturn(Val_int(ChannelLayout_val(_channel_layout)));
}


/**** Sample format ****/

static const enum AVSampleFormat SAMPLE_FORMATS[] = {
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
#define SAMPLE_FORMATS_LEN (sizeof(SAMPLE_FORMATS) / sizeof(enum AVSampleFormat))

static const enum caml_ba_kind BIGARRAY_KINDS[SAMPLE_FORMATS_LEN] = {
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

enum caml_ba_kind bigarray_kind_of_AVSampleFormat(enum AVSampleFormat sf)
{
  int i;
  for (i = 0; i < SAMPLE_FORMATS_LEN; i++)
    {
      if (sf == SAMPLE_FORMATS[i])
        return BIGARRAY_KINDS[i];
    }
  return CAML_BA_KIND_MASK;
}

CAMLprim value ocaml_avutil_find_sample_fmt(value _name)
{
  CAMLparam1(_name);
  CAMLlocal1(ans);
  char *name = strndup(String_val(_name), caml_string_length(_name));
  if (!name) caml_raise_out_of_memory();

  caml_release_runtime_system();
  enum AVSampleFormat ret = av_get_sample_fmt(name);
  caml_acquire_runtime_system();

  free(name);

  if (ret == AV_SAMPLE_FMT_NONE) caml_raise_not_found();

  CAMLreturn(Val_SampleFormat(ret));
}

CAMLprim value ocaml_avutil_get_sample_fmt_name(value _sample_fmt)
{
  CAMLparam1(_sample_fmt);
  CAMLlocal1(ans);

  caml_release_runtime_system();
  const char *name = av_get_sample_fmt_name(SampleFormat_val(_sample_fmt));
  caml_acquire_runtime_system();

  ans = caml_copy_string(name);

  CAMLreturn(ans);
}

CAMLprim value ocaml_avutil_get_sample_fmt_id(value _sample_fmt)
{
  CAMLparam1(_sample_fmt);
  CAMLreturn(Val_int(SampleFormat_val(_sample_fmt)));
}

CAMLprim value ocaml_avutil_find_sample_fmt_from_id(value _id)
{
  CAMLparam0();
  value ret = Val_SampleFormat(Int_val(_id));

  if (ret == VALUE_NOT_FOUND) caml_raise_not_found();

  CAMLreturn(ret);
}

/***** AVPixelFormat *****/
CAMLprim value ocaml_avutil_pixelformat_bits_per_pixel(value pixel)
{
  CAMLparam1(pixel);
  enum AVPixelFormat p = PixelFormat_val(pixel);

  CAMLreturn(Val_int(av_get_bits_per_pixel(av_pix_fmt_desc_get(p))));
}

CAMLprim value ocaml_avutil_pixelformat_planes(value pixel)
{
  CAMLparam1(pixel);
  enum AVPixelFormat p = PixelFormat_val(pixel);

  CAMLreturn(Val_int(av_pix_fmt_count_planes(p)));
}

CAMLprim value ocaml_avutil_pixelformat_to_string(value pixel)
{
  CAMLparam1(pixel);
  enum AVPixelFormat p = PixelFormat_val(pixel);

  CAMLreturn(caml_copy_string(av_get_pix_fmt_name(p)));
}

CAMLprim value ocaml_avutil_pixelformat_of_string(value name)
{
  CAMLparam1(name);
 
  caml_release_runtime_system();
  enum AVPixelFormat p = av_get_pix_fmt(String_val(name));
  caml_acquire_runtime_system();

  if (p == AV_PIX_FMT_NONE) Fail( "Invalid format name");

  CAMLreturn(Val_PixelFormat(p));
}


/***** AVFrame *****/

CAMLprim value ocaml_avutil_finalize_frame(value v)
{
  CAMLparam1(v);
  caml_register_generational_global_root(&v);
#ifdef HAS_FRAME
  AVFrame *frame = Frame_val(v);
  caml_release_runtime_system();
  av_frame_free(&frame);
  caml_acquire_runtime_system();
#endif
  caml_remove_generational_global_root(&v);
  CAMLreturn(Val_unit);
}

static struct custom_operations frame_ops =
  {
    "ocaml_avframe",
    custom_finalize_default,
    custom_compare_default,
    custom_hash_default,
    custom_serialize_default,
    custom_deserialize_default
  };

value value_of_frame(AVFrame *frame)
{
  value ret;
  if(!frame) Fail( "Empty frame");

  ret = caml_alloc_custom(&frame_ops, sizeof(AVFrame*), 0, 1);
  Frame_val(ret) = frame;

  Finalize("ocaml_avutil_finalize_frame",ret);

  return ret;
}

CAMLprim value ocaml_avutil_video_create_frame(value _w, value _h, value _format)
{
  CAMLparam1(_format);
  CAMLlocal1(ans);
#ifndef HAS_FRAME
  caml_failwith("Not implemented.");
#else
  caml_release_runtime_system();
  AVFrame *frame = av_frame_alloc();
  caml_acquire_runtime_system();
  if( ! frame) caml_raise_out_of_memory();

  frame->format = PixelFormat_val(_format);
  frame->width  = Int_val(_w);
  frame->height = Int_val(_h);

  caml_release_runtime_system();
  int ret = av_frame_get_buffer(frame, 32);
  caml_acquire_runtime_system();

  if(ret < 0) {
    av_frame_free(&frame);
    ocaml_avutil_raise_error(ret);
  }

  ans = value_of_frame(frame);
#endif
  CAMLreturn(ans);
}

CAMLprim value ocaml_avutil_video_frame_get_sample_format(value _frame)
{
  CAMLparam1(_frame);
#ifdef HAS_FRAME
  AVFrame *frame = Frame_val(_frame);

  CAMLreturn(Val_SampleFormat((enum AVSampleFormat)frame->format));
#else
  caml_failwith("Not implemented.");
  CAMLreturn(Val_unit);
#endif
}

CAMLprim value ocaml_avutil_video_frame_get_linesize(value _frame, value _line)
{
  CAMLparam1(_frame);
#ifdef HAS_FRAME
  AVFrame *frame = Frame_val(_frame);
  int line = Int_val(_line);

  if(line < 0 || line >= AV_NUM_DATA_POINTERS || ! frame->data[line]) Fail( "Failed to get linesize from video frame : line (%d) out of boundaries", line);

  CAMLreturn(Val_int(frame->linesize[line]));
#else
  caml_failwith("Not implemented.");
  CAMLreturn(Val_unit);
#endif
}

CAMLprim value ocaml_avutil_video_get_frame_bigarray_planes(value _frame, value _make_writable)
{
  CAMLparam1(_frame);
  CAMLlocal2(ans, plane);
#ifndef HAS_FRAME
  caml_failwith("Not implemented.");
#else
  AVFrame *frame = Frame_val(_frame);
  int i;

  if(Bool_val(_make_writable)) {
    caml_release_runtime_system();
    int ret = av_frame_make_writable(frame);
    caml_acquire_runtime_system();

    if (ret < 0) ocaml_avutil_raise_error(ret);
  }

  caml_release_runtime_system();
  int nb_planes = av_pix_fmt_count_planes((enum AVPixelFormat)frame->format);
  caml_acquire_runtime_system();

  if(nb_planes < 0) ocaml_avutil_raise_error(nb_planes);
  
  ans = caml_alloc_tuple(nb_planes);

  for(i = 0; i < nb_planes; i++) {
    intnat out_size = frame->linesize[i] * frame->height;
    plane = caml_alloc_tuple(2);

    Store_field(plane, 0, caml_ba_alloc(CAML_BA_C_LAYOUT | CAML_BA_UINT8, 1, frame->data[i], &out_size));
    Store_field(plane, 1, Val_int(frame->linesize[i]));
    Store_field(ans, i, plane);
  }
#endif
  CAMLreturn(ans);
}

CAMLprim value ocaml_avutil_video_frame_pts(value _frame)
{
  CAMLparam1(_frame);
#ifdef HAS_FRAME
  AVFrame *frame = Frame_val(_frame);

  CAMLreturn(caml_copy_int64(frame->pts));
#else
  caml_failwith("Not implemented.");
  CAMLreturn(Val_unit);
#endif
}


/***** AVSubtitle *****/

CAMLprim value ocaml_avutil_finalize_subtitle(value v)
{
  CAMLparam1(v);
  
  caml_register_generational_global_root(&v);

  struct AVSubtitle *subtitle = Subtitle_val(v);

  caml_release_runtime_system();
  avsubtitle_free(subtitle);
  caml_acquire_runtime_system();

  free(subtitle);

  caml_remove_generational_global_root(&v);
 
  CAMLreturn(Val_unit);
}

static struct custom_operations subtitle_ops =
  {
    "ocaml_avsubtitle",
    custom_finalize_default,
    custom_compare_default,
    custom_hash_default,
    custom_serialize_default,
    custom_deserialize_default
  };

value value_of_subtitle(AVSubtitle *subtitle)
{
  value ret;
  if( ! subtitle) Fail( "Empty subtitle");

  ret = caml_alloc_custom(&subtitle_ops, sizeof(AVSubtitle*), 0, 1);
  Subtitle_val(ret) = subtitle;

  Finalize("ocaml_avutil_finalize_subtitle", ret);

  return ret;
}

int subtitle_header_default(AVCodecContext *codec_context)
{
  return 0;
}

CAMLprim value ocaml_avutil_subtitle_create_frame(value _start_time, value _end_time, value _lines)
{
  CAMLparam3(_start_time, _end_time, _lines);
  CAMLlocal1(ans);
  int64_t start_time = Int64_val(_start_time);
  int64_t end_time = Int64_val(_end_time);
  int nb_lines = Wosize_val(_lines);

  AVSubtitle * subtitle = (AVSubtitle*)calloc(1, sizeof(AVSubtitle));
  if( ! subtitle) caml_raise_out_of_memory();

  ans = value_of_subtitle(subtitle);

  //  subtitle->start_display_time = (uint32_t)start_time;
  subtitle->end_display_time = (uint32_t)end_time;
  subtitle->pts = start_time;

  caml_release_runtime_system();
  subtitle->rects = (AVSubtitleRect **)av_malloc_array(nb_lines, sizeof(AVSubtitleRect *));
  caml_acquire_runtime_system();

  if( ! subtitle->rects) caml_raise_out_of_memory();

  subtitle->num_rects = nb_lines;
  
  int i;
  for(i = 0; i < nb_lines; i++) {
    const char * text = String_val(Field(_lines, i));
    
    caml_release_runtime_system();
    subtitle->rects[i] = (AVSubtitleRect *)av_mallocz(sizeof(AVSubtitleRect));
    caml_acquire_runtime_system();
  
    if( ! subtitle->rects[i]) caml_raise_out_of_memory();

    subtitle->rects[i]->type = SUBTITLE_TEXT;
    subtitle->rects[i]->text = strdup(text);
    if( ! subtitle->rects[i]->text) caml_raise_out_of_memory();

    //    subtitle->rects[i]->type = SUBTITLE_ASS;
    //    subtitle->rects[i]->ass = get_dialog(i + 1, 0, NULL, NULL, text);
    //    if( ! subtitle->rects[i]->ass) Fail( "Failed to allocate subtitle frame");
  }

  CAMLreturn(ans);
}

CAMLprim value ocaml_avutil_subtitle_to_lines(value _subtitle)
{
  CAMLparam1(_subtitle);
  CAMLlocal2(ans, lines);
  struct AVSubtitle *subtitle = Subtitle_val(_subtitle);
  unsigned i, num_rects = subtitle->num_rects;

  lines = caml_alloc_tuple(num_rects);

  for(i = 0; i < num_rects; i++) {
    char * line = subtitle->rects[i]->text ? subtitle->rects[i]->text : subtitle->rects[i]->ass;
    Store_field(lines, i, caml_copy_string(line));
  }

  ans = caml_alloc_tuple(3);
  Store_field(ans, 0, caml_copy_int64((int64_t)subtitle->start_display_time));
  Store_field(ans, 1, caml_copy_int64((int64_t)subtitle->end_display_time));
  Store_field(ans, 2, lines);

  CAMLreturn(ans);
}
