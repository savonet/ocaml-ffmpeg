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

char ocaml_av_error_msg[ERROR_MSG_SIZE + 1];
char ocaml_av_exn_msg[ERROR_MSG_SIZE + 1];


/***** Global initialisation *****/

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
  void *ptr;

  pthread_once(&ocaml_c_thread_key_once, ocaml_ffmpeg_make_key);

  if (caml_c_thread_register() && !pthread_getspecific(ocaml_c_thread_key))
    pthread_setspecific(ocaml_c_thread_key,(void*)&initialized);
}

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

int register_lock_manager()
{
  static int registering_done = 0;
  static pthread_mutex_t registering_mutex = PTHREAD_MUTEX_INITIALIZER;

  if( ! registering_done) {
    pthread_mutex_lock(&registering_mutex);
 
    if( ! registering_done) {

      int ret = av_lockmgr_register(lock_manager);

      if(ret < 0) {
        Log("Failed to register lock manager : %s", av_err2str(ret));
      }
      else {
        registering_done = 1;
      }
      pthread_mutex_unlock(&registering_mutex);
    }
  }
  return registering_done;
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
  value ret, buffer;

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

  av_log_set_callback(&av_log_ocaml_callback);

  CAMLreturn(Val_unit);
}

CAMLprim value ocaml_avutil_clear_log_callback()
{
  CAMLparam0();

  if (ocaml_log_callback != (value)NULL) {
    caml_remove_generational_global_root(&ocaml_log_callback);
    ocaml_log_callback = (value)NULL;
  }

  av_log_set_callback(&av_log_default_callback);

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
CAMLprim value ocaml_avutil_get_channel_layout_nb_channels(value _channel_layout)
{
  CAMLparam1(_channel_layout);
  CAMLreturn(Val_int(av_get_channel_layout_nb_channels(ChannelLayout_val(_channel_layout))));
}

CAMLprim value ocaml_avutil_get_default_channel_layout(value _nb_channels)
{
  CAMLparam1(_nb_channels);
  CAMLreturn(Val_ChannelLayout(av_get_default_channel_layout(Int_val(_nb_channels))));
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

CAMLprim value ocaml_avutil_get_sample_fmt_name(value _sample_fmt)
{
  CAMLparam1(_sample_fmt);
  CAMLlocal1(ans);

  const char *name = av_get_sample_fmt_name(SampleFormat_val(_sample_fmt));
  ans = caml_copy_string(name);

  CAMLreturn(ans);
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
  enum AVPixelFormat p = av_get_pix_fmt(String_val(name));

  if (p == AV_PIX_FMT_NONE) Raise(EXN_FAILURE, "Invalid format name");

  CAMLreturn(Val_PixelFormat(p));
}


/***** AVFrame *****/

static void finalize_frame(value v)
{
#ifdef HAS_FRAME
  AVFrame *frame = Frame_val(v);
  if(frame) av_frame_free(&frame);
#endif
}

static struct custom_operations frame_ops =
  {
    "ocaml_avframe",
    finalize_frame,
    custom_compare_default,
    custom_hash_default,
    custom_serialize_default,
    custom_deserialize_default
  };

void value_of_frame(AVFrame *frame, value * pvalue)
{
  if( ! frame) Raise(EXN_FAILURE, "Empty frame");

  *pvalue = caml_alloc_custom(&frame_ops, sizeof(AVFrame*), 0, 1);
  Frame_val((*pvalue)) = frame;
}

AVFrame * alloc_frame_value(value * pvalue)
{
  AVFrame * frame = av_frame_alloc();
  if( ! frame) Fail("Failed to allocate frame");

  value_of_frame(frame, pvalue);
  return frame;
}


CAMLprim value ocaml_avutil_video_create_frame(value _w, value _h, value _format)
{
  CAMLparam1(_format);
  CAMLlocal1(ans);
#ifndef HAS_FRAME
  caml_failwith("Not implemented.");
#else
  AVFrame *frame = av_frame_alloc();
  if( ! frame) Raise(EXN_FAILURE, "Failed to alloc video frame");

  frame->format = PixelFormat_val(_format);
  frame->width  = Int_val(_w);
  frame->height = Int_val(_h);

  int ret = av_frame_get_buffer(frame, 32);
  if(ret < 0) Raise(EXN_FAILURE, "Failed to alloc video frame buffer : %s", av_err2str(ret));

  value_of_frame(frame, &ans);
#endif
  CAMLreturn(ans);
}

CAMLprim value ocaml_avutil_video_frame_get_linesize(value _frame, value _line)
{
  CAMLparam1(_frame);
#ifdef HAS_FRAME
  AVFrame *frame = Frame_val(_frame);
  int line = Int_val(_line);

  if(line < 0 || line >= AV_NUM_DATA_POINTERS || ! frame->data[line]) Raise(EXN_FAILURE, "Failed to get linesize from video frame : line (%d) out of boundaries", line);

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
    int ret = av_frame_make_writable(frame);
    if (ret < 0) Raise(EXN_FAILURE, "Failed to make frame writable : %s", av_err2str(ret));
  }

  int nb_planes = av_pix_fmt_count_planes((enum AVPixelFormat)frame->format);
  if(nb_planes < 0) Raise(EXN_FAILURE, "Failed to get frame planes count : %s", av_err2str(nb_planes));
  
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


/***** AVSubtitle *****/

static void finalize_subtitle(value v)
{
  struct AVSubtitle *subtitle = Subtitle_val(v);

  avsubtitle_free(subtitle);
  free(subtitle);
}

static struct custom_operations subtitle_ops =
  {
    "ocaml_avsubtitle",
    finalize_subtitle,
    custom_compare_default,
    custom_hash_default,
    custom_serialize_default,
    custom_deserialize_default
  };

void value_of_subtitle(AVSubtitle *subtitle, value * pvalue)
{
  if( ! subtitle) Raise(EXN_FAILURE, "Empty subtitle");

  *pvalue = caml_alloc_custom(&subtitle_ops, sizeof(AVSubtitle*), 0, 1);
  Subtitle_val((*pvalue)) = subtitle;
}

AVSubtitle * alloc_subtitle_value(value * pvalue)
{
  AVSubtitle * subtitle = (AVSubtitle*)calloc(1, sizeof(AVSubtitle));
  if( ! subtitle) Fail("Failed to allocate subtitle");

  value_of_subtitle(subtitle, pvalue);

  return subtitle;
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
  if( ! subtitle) Raise(EXN_FAILURE, "Failed to allocate subtitle frame");

  value_of_subtitle(subtitle, &ans);

  //  subtitle->start_display_time = (uint32_t)start_time;
  subtitle->end_display_time = (uint32_t)end_time;
  subtitle->pts = start_time;

  subtitle->rects = (AVSubtitleRect **)av_malloc_array(nb_lines, sizeof(AVSubtitleRect *));
  if( ! subtitle->rects) Raise(EXN_FAILURE, "Failed to allocate subtitle frame");

  subtitle->num_rects = nb_lines;
  
  int i;
  for(i = 0; i < nb_lines; i++) {
    const char * text = String_val(Field(_lines, i));
    subtitle->rects[i] = (AVSubtitleRect *)av_mallocz(sizeof(AVSubtitleRect));
    if( ! subtitle->rects[i]) Raise(EXN_FAILURE, "Failed to allocate subtitle frame");

    subtitle->rects[i]->type = SUBTITLE_TEXT;
    subtitle->rects[i]->text = strdup(text);
    if( ! subtitle->rects[i]->text) Raise(EXN_FAILURE, "Failed to allocate subtitle frame");

    //    subtitle->rects[i]->type = SUBTITLE_ASS;
    //    subtitle->rects[i]->ass = get_dialog(i + 1, 0, NULL, NULL, text);
    //    if( ! subtitle->rects[i]->ass) Raise(EXN_FAILURE, "Failed to allocate subtitle frame");
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

