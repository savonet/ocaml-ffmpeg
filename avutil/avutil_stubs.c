#include <assert.h>
#include <pthread.h>

#include <caml/alloc.h>
#include <caml/bigarray.h>
#include <caml/callback.h>
#include <caml/custom.h>
#include <caml/fail.h>
#include <caml/memory.h>
#include <caml/mlvalues.h>
#include <caml/threads.h>

#include <libavutil/avassert.h>
#include <libavutil/avstring.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <libavutil/pixfmt.h>

#include "avutil_stubs.h"
#include "channel_layout_stubs.h"
#include "pixel_format_stubs.h"
#include "sample_format_stubs.h"

char ocaml_av_exn_msg[ERROR_MSG_SIZE + 1];

void ocaml_avutil_raise_error(int err) {
  value _err;

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
    _err = caml_alloc_tuple(2);
    Store_field(_err, 0, PVV_Other);
    Store_field(_err, 1, Val_int(err));
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
    if (Field(error, 0) == PVV_Other)
      err = Int_val(Field(error, 1));
    else
      // Failure
      CAMLreturn(Field(error, 1));
  }

  CAMLreturn(caml_copy_string(av_err2str(err)));
}

/***** Global initialisation *****/
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58, 9, 100)
static int lock_manager(void **mtx, enum AVLockOp op) {
  switch (op) {
  case AV_LOCK_CREATE:
    *mtx = malloc(sizeof(pthread_mutex_t));

    if (!*mtx)
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

CAMLprim value ocaml_avutil_register_lock_manager(value unit) {
  CAMLparam0();
  static int registering_done = 0;
  static pthread_mutex_t registering_mutex = PTHREAD_MUTEX_INITIALIZER;

  if (!registering_done) {
    pthread_mutex_lock(&registering_mutex);

    if (!registering_done) {

      caml_release_runtime_system();
      int ret = av_lockmgr_register(lock_manager);
      caml_acquire_runtime_system();

      if (ret >= 0) {
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
    pthread_setspecific(ocaml_c_thread_key, (void *)&initialized);
}

/**** Rational ****/
void value_of_rational(const AVRational *rational, value *pvalue) {
  *pvalue = caml_alloc_tuple(2);
  Field(*pvalue, 0) = Val_int(rational->num);
  Field(*pvalue, 1) = Val_int(rational->den);
}

value ocaml_avutil_av_d2q(value f) {
  CAMLparam1(f);
  CAMLlocal1(ret);

  const AVRational r = av_d2q(Double_val(f), INT_MAX);
  ;
  value_of_rational(&r, &ret);

  CAMLreturn(ret);
}

/**** Time format ****/
int64_t second_fractions_of_time_format(value time_format) {
  switch (time_format) {
  case PVV_Second:
    return 1;
  case PVV_Millisecond:
    return 1000;
  case PVV_Microsecond:
    return 1000000;
  case PVV_Nanosecond:
    return 1000000000;
  default:
    break;
  }
  return 1;
}

/**** Logging ****/
CAMLprim value ocaml_avutil_set_log_level(value level) {
  CAMLparam0();
  av_log_set_level(Int_val(level));
  CAMLreturn(Val_unit);
}

#define LINE_SIZE 1024

typedef struct {
  char msg[LINE_SIZE];
  void *next;
} log_msg_t;

static pthread_cond_t log_condition = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
static log_msg_t top_level_log_msg = {"", NULL};

CAMLprim value ocaml_ffmpeg_process_log(value cb) {
  CAMLparam1(cb);
  CAMLlocal1(buffer);
  log_msg_t *log_msg, *next_log_msg;

  while (1) {
    caml_release_runtime_system();
    pthread_mutex_lock(&log_mutex);

    while (top_level_log_msg.next == NULL)
      pthread_cond_wait(&log_condition, &log_mutex);

    log_msg = top_level_log_msg.next;
    top_level_log_msg.next = NULL;
    pthread_mutex_unlock(&log_mutex);

    caml_acquire_runtime_system();

    while (log_msg != NULL) {
      buffer = caml_copy_string(log_msg->msg);
      caml_callback(cb, buffer);

      next_log_msg = log_msg->next;
      free(log_msg);
      log_msg = next_log_msg;
    }
  }

  CAMLreturn(Val_unit);
}

static void av_log_ocaml_callback(void *ptr, int level, const char *fmt,
                                  va_list vl) {
  static int print_prefix = 1;
  log_msg_t *log_msg;

  if (level > av_log_get_level())
    return;

  pthread_mutex_lock(&log_mutex);

  log_msg = &top_level_log_msg;

  while (log_msg->next != NULL) {
    log_msg = log_msg->next;
  }

  // TODO: check for NULL here
  log_msg->next = malloc(sizeof(log_msg_t));

  log_msg = (log_msg_t *)log_msg->next;
  log_msg->next = NULL;
  av_log_format_line2(ptr, level, fmt, vl, log_msg->msg, LINE_SIZE,
                      &print_prefix);

  pthread_cond_signal(&log_condition);
  pthread_mutex_unlock(&log_mutex);
}

CAMLprim value ocaml_avutil_setup_log_callback(value unit) {
  CAMLparam0();

  caml_release_runtime_system();
  av_log_set_callback(&av_log_ocaml_callback);
  caml_acquire_runtime_system();

  CAMLreturn(Val_unit);
}

CAMLprim value ocaml_avutil_clear_log_callback() {
  CAMLparam0();

  caml_release_runtime_system();
  av_log_set_callback(&av_log_default_callback);
  caml_acquire_runtime_system();

  CAMLreturn(Val_unit);
}

CAMLprim value ocaml_avutil_time_base() {
  CAMLparam0();
  CAMLlocal1(ans);

  value_of_rational(&AV_TIME_BASE_Q, &ans);

  CAMLreturn(ans);
}

/**** Channel layout ****/
CAMLprim value ocaml_avutil_get_channel_layout_description(
    value _channel_layout, value channels) {
  CAMLparam1(_channel_layout);
  char buf[1024];
  uint64_t channel_layout = ChannelLayout_val(_channel_layout);

  caml_release_runtime_system();
  av_get_channel_layout_string(buf, sizeof(buf), Int_val(channels),
                               channel_layout);
  caml_acquire_runtime_system();

  CAMLreturn(caml_copy_string(buf));
}

CAMLprim value
ocaml_avutil_get_channel_layout_nb_channels(value _channel_layout) {
  CAMLparam1(_channel_layout);
  CAMLreturn(Val_int(
      av_get_channel_layout_nb_channels(ChannelLayout_val(_channel_layout))));
}

CAMLprim value ocaml_avutil_get_default_channel_layout(value _nb_channels) {
  CAMLparam0();

  caml_release_runtime_system();
  int64_t ret = av_get_default_channel_layout(Int_val(_nb_channels));
  caml_acquire_runtime_system();

  if (ret == 0)
    caml_raise_not_found();

  CAMLreturn(Val_ChannelLayout(ret));
}

CAMLprim value ocaml_avutil_get_channel_layout(value _name) {
  CAMLparam1(_name);
  char *name = strndup(String_val(_name), caml_string_length(_name));

  if (!name)
    caml_raise_out_of_memory();

  caml_release_runtime_system();
  int64_t ret = av_get_channel_layout(name);
  caml_acquire_runtime_system();

  free(name);

  if (ret == 0)
    caml_raise_not_found();

  CAMLreturn(Val_ChannelLayout(ret));
}

CAMLprim value ocaml_avutil_get_channel_layout_id(value _channel_layout) {
  CAMLparam1(_channel_layout);
  CAMLreturn(caml_copy_int64(ChannelLayout_val(_channel_layout)));
}

CAMLprim value ocaml_avutil_channel_layout_of_id(value v) {
  CAMLparam0();
  CAMLreturn(Val_ChannelLayout(Int64_val(v)));
}

/**** Sample format ****/

static const enum AVSampleFormat SAMPLE_FORMATS[] = {
    AV_SAMPLE_FMT_NONE, AV_SAMPLE_FMT_U8,   AV_SAMPLE_FMT_S16,
    AV_SAMPLE_FMT_S32,  AV_SAMPLE_FMT_FLT,  AV_SAMPLE_FMT_DBL,
    AV_SAMPLE_FMT_U8P,  AV_SAMPLE_FMT_S16P, AV_SAMPLE_FMT_S32P,
    AV_SAMPLE_FMT_FLTP, AV_SAMPLE_FMT_DBLP};
#define SAMPLE_FORMATS_LEN                                                     \
  (sizeof(SAMPLE_FORMATS) / sizeof(enum AVSampleFormat))

static const enum caml_ba_kind BIGARRAY_KINDS[SAMPLE_FORMATS_LEN] = {
    CAML_BA_KIND_MASK, CAML_BA_UINT8,   CAML_BA_SINT16, CAML_BA_INT32,
    CAML_BA_FLOAT32,   CAML_BA_FLOAT64, CAML_BA_UINT8,  CAML_BA_SINT16,
    CAML_BA_INT32,     CAML_BA_FLOAT32, CAML_BA_FLOAT64};

enum caml_ba_kind bigarray_kind_of_AVSampleFormat(enum AVSampleFormat sf) {
  int i;
  for (i = 0; i < SAMPLE_FORMATS_LEN; i++) {
    if (sf == SAMPLE_FORMATS[i])
      return BIGARRAY_KINDS[i];
  }
  return CAML_BA_KIND_MASK;
}

CAMLprim value ocaml_avutil_find_sample_fmt(value _name) {
  CAMLparam1(_name);
  CAMLlocal1(ans);
  char *name = strndup(String_val(_name), caml_string_length(_name));
  if (!name)
    caml_raise_out_of_memory();

  caml_release_runtime_system();
  enum AVSampleFormat ret = av_get_sample_fmt(name);
  caml_acquire_runtime_system();

  free(name);

  if (ret == AV_SAMPLE_FMT_NONE)
    caml_raise_not_found();

  CAMLreturn(Val_SampleFormat(ret));
}

CAMLprim value ocaml_avutil_get_sample_fmt_name(value _sample_fmt) {
  CAMLparam1(_sample_fmt);
  CAMLlocal1(ans);

  caml_release_runtime_system();
  const char *name = av_get_sample_fmt_name(SampleFormat_val(_sample_fmt));
  caml_acquire_runtime_system();

  ans = caml_copy_string(name);

  CAMLreturn(ans);
}

CAMLprim value ocaml_avutil_get_sample_fmt_id(value _sample_fmt) {
  CAMLparam1(_sample_fmt);
  CAMLreturn(Val_int(SampleFormat_val(_sample_fmt)));
}

CAMLprim value ocaml_avutil_find_sample_fmt_from_id(value _id) {
  CAMLparam0();
  value ret = Val_SampleFormat(Int_val(_id));

  if (ret == VALUE_NOT_FOUND)
    caml_raise_not_found();

  CAMLreturn(ret);
}

/***** AVPixelFormat *****/
CAMLprim value ocaml_avutil_pixelformat_bits_per_pixel(value pixel) {
  CAMLparam1(pixel);
  enum AVPixelFormat p = PixelFormat_val(pixel);

  CAMLreturn(Val_int(av_get_bits_per_pixel(av_pix_fmt_desc_get(p))));
}

CAMLprim value ocaml_avutil_pixelformat_planes(value pixel) {
  CAMLparam1(pixel);
  enum AVPixelFormat p = PixelFormat_val(pixel);

  CAMLreturn(Val_int(av_pix_fmt_count_planes(p)));
}

CAMLprim value ocaml_avutil_pixelformat_of_int(value v) {
  CAMLparam0();
  CAMLreturn(Val_PixelFormat((enum AVPixelFormat)Int_val(v)));
}

CAMLprim value ocaml_avutil_pixelformat_to_string(value pixel) {
  CAMLparam1(pixel);
  enum AVPixelFormat p = PixelFormat_val(pixel);

  CAMLreturn(caml_copy_string(av_get_pix_fmt_name(p)));
}

CAMLprim value ocaml_avutil_pixelformat_of_string(value name) {
  CAMLparam1(name);

  caml_release_runtime_system();
  enum AVPixelFormat p = av_get_pix_fmt(String_val(name));
  caml_acquire_runtime_system();

  if (p == AV_PIX_FMT_NONE)
    Fail("Invalid format name");

  CAMLreturn(Val_PixelFormat(p));
}

/***** AVFrame *****/

static void finalize_frame(value v) {
  AVFrame *frame = Frame_val(v);
  av_frame_free(&frame);
}

static struct custom_operations frame_ops = {
    "ocaml_avframe",     finalize_frame,           custom_compare_default,
    custom_hash_default, custom_serialize_default, custom_deserialize_default};

value value_of_frame(AVFrame *frame) {
  value ret;
  if (!frame)
    Fail("Empty frame");

  ret = caml_alloc_custom(&frame_ops, sizeof(AVFrame *), 0, 1);
  Frame_val(ret) = frame;

  return ret;
}

CAMLprim value ocaml_avutil_frame_pkt_duration(value _frame) {
  CAMLparam1(_frame);
  CAMLlocal1(ret);
  AVFrame *frame = Frame_val(_frame);

  if (frame->pkt_duration == 0)
    CAMLreturn(Val_none);

  ret = caml_alloc_tuple(1);
  Store_field(ret, 0, caml_copy_int64(frame->pkt_duration));

  CAMLreturn(ret);
}

CAMLprim value ocaml_avutil_frame_pts(value _frame) {
  CAMLparam1(_frame);
  CAMLlocal1(ret);
  AVFrame *frame = Frame_val(_frame);

  if (frame->pts == AV_NOPTS_VALUE)
    CAMLreturn(Val_none);

  ret = caml_alloc_tuple(1);
  Store_field(ret, 0, caml_copy_int64(frame->pts));

  CAMLreturn(ret);
}

CAMLprim value ocaml_avutil_frame_set_pts(value _frame, value _pts) {
  CAMLparam2(_frame, _pts);
  AVFrame *frame = Frame_val(_frame);

  if (_pts == Val_none)
    frame->pts = AV_NOPTS_VALUE;
  else
    frame->pts = Int64_val(Field(_pts, 0));

  CAMLreturn(Val_unit);
}

CAMLprim value ocaml_avutil_frame_copy(value _src, value _dst) {
  CAMLparam2(_src, _dst);
  AVFrame *src = Frame_val(_src);
  AVFrame *dst = Frame_val(_dst);
  int ret;

  caml_release_runtime_system();
  ret = av_frame_copy(dst, src);
  caml_acquire_runtime_system();

  if (ret < 0)
    ocaml_avutil_raise_error(ret);

  CAMLreturn(Val_unit);
}

CAMLprim value ocaml_avutil_video_create_frame(value _w, value _h,
                                               value _format) {
  CAMLparam1(_format);
  caml_release_runtime_system();
  AVFrame *frame = av_frame_alloc();
  caml_acquire_runtime_system();
  if (!frame)
    caml_raise_out_of_memory();

  frame->format = PixelFormat_val(_format);
  frame->width = Int_val(_w);
  frame->height = Int_val(_h);

  caml_release_runtime_system();
  int ret = av_frame_get_buffer(frame, 32);
  caml_acquire_runtime_system();

  if (ret < 0) {
    av_frame_free(&frame);
    ocaml_avutil_raise_error(ret);
  }

  CAMLreturn(value_of_frame(frame));
}

/* Adapted from alloc_audio_frame */
CAMLprim value ocaml_avutil_audio_create_frame(value _sample_fmt,
                                               value _channel_layout,
                                               value _samplerate,
                                               value _samples) {
  CAMLparam2(_sample_fmt, _channel_layout);
  enum AVSampleFormat sample_fmt = SampleFormat_val(_sample_fmt);
  uint64_t channel_layout = ChannelLayout_val(_channel_layout);
  int sample_rate = Int_val(_samplerate);
  int nb_samples = Int_val(_samples);
  int ret;

  caml_release_runtime_system();
  AVFrame *frame = av_frame_alloc();
  caml_acquire_runtime_system();

  if (!frame)
    caml_raise_out_of_memory();

  frame->format = sample_fmt;
  frame->channel_layout = channel_layout;
  frame->sample_rate = sample_rate;
  frame->nb_samples = nb_samples;

  caml_release_runtime_system();
  ret = av_frame_get_buffer(frame, 0);
  caml_acquire_runtime_system();

  if (ret < 0) {
    av_frame_free(&frame);
    ocaml_avutil_raise_error(ret);
  }

  CAMLreturn(value_of_frame(frame));
}

CAMLprim value ocaml_avutil_audio_frame_get_sample_format(value _frame) {
  CAMLparam1(_frame);
  AVFrame *frame = Frame_val(_frame);

  CAMLreturn(Val_SampleFormat((enum AVSampleFormat)frame->format));
}

CAMLprim value ocaml_avutil_audio_frame_get_sample_rate(value _frame) {
  CAMLparam1(_frame);
  AVFrame *frame = Frame_val(_frame);

  CAMLreturn(Val_int(frame->sample_rate));
}

CAMLprim value ocaml_avutil_audio_frame_get_channels(value _frame) {
  CAMLparam1(_frame);
  AVFrame *frame = Frame_val(_frame);

  CAMLreturn(Val_int(frame->channels));
}

CAMLprim value ocaml_avutil_audio_frame_get_channel_layout(value _frame) {
  CAMLparam1(_frame);
  AVFrame *frame = Frame_val(_frame);

  CAMLreturn(Val_ChannelLayout((frame->channel_layout)));
}

CAMLprim value ocaml_avutil_audio_frame_nb_samples(value _frame) {
  CAMLparam1(_frame);
  AVFrame *frame = Frame_val(_frame);

  CAMLreturn(Val_int(frame->nb_samples));
}

/* Adapted from frame_copy_audio */
CAMLprim value ocaml_avutil_audio_frame_copy_samples(value _src, value _src_ofs,
                                                     value _dst, value _dst_ofs,
                                                     value _len) {
  CAMLparam2(_src, _dst);
  AVFrame *src = Frame_val(_src);
  AVFrame *dst = Frame_val(_dst);
  int src_ofs = Int_val(_src_ofs);
  int dst_ofs = Int_val(_dst_ofs);
  int len = Int_val(_len);

  int planar = av_sample_fmt_is_planar(dst->format);
  int channels = dst->channels;
  int planes = planar ? channels : 1;
  int i;

  if (src->nb_samples < src_ofs + len || dst->nb_samples < dst_ofs + len ||
      dst->channels != src->channels ||
      dst->channel_layout != src->channel_layout)
    ocaml_avutil_raise_error(AVERROR(EINVAL));

  av_assert2(!src->channel_layout ||
             src->channels ==
                 av_get_channel_layout_nb_channels(src->channel_layout));

  for (i = 0; i < planes; i++)
    if (!dst->extended_data[i] || !src->extended_data[i])
      ocaml_avutil_raise_error(AVERROR(EINVAL));

  caml_release_runtime_system();
  av_samples_copy(dst->extended_data, src->extended_data, dst_ofs, src_ofs, len,
                  channels, dst->format);
  caml_acquire_runtime_system();

  CAMLreturn(Val_unit);
}

CAMLprim value ocaml_avutil_video_frame_width(value _frame) {
  CAMLparam1(_frame);

  AVFrame *frame = Frame_val(_frame);

  CAMLreturn(Val_int(frame->width));
}

CAMLprim value ocaml_avutil_video_frame_height(value _frame) {
  CAMLparam1(_frame);

  AVFrame *frame = Frame_val(_frame);

  CAMLreturn(Val_int(frame->height));
}

CAMLprim value ocaml_avutil_video_frame_get_pixel_format(value _frame) {
  CAMLparam1(_frame);
  AVFrame *frame = Frame_val(_frame);

  CAMLreturn(Val_PixelFormat(frame->format));
}

CAMLprim value ocaml_avutil_video_frame_get_linesize(value _frame,
                                                     value _line) {
  CAMLparam1(_frame);
  AVFrame *frame = Frame_val(_frame);
  int line = Int_val(_line);

  if (line < 0 || line >= AV_NUM_DATA_POINTERS || !frame->data[line])
    Fail(
        "Failed to get linesize from video frame : line (%d) out of boundaries",
        line);

  CAMLreturn(Val_int(frame->linesize[line]));
}

CAMLprim value ocaml_avutil_video_get_frame_bigarray_planes(
    value _frame, value _make_writable) {
  CAMLparam1(_frame);
  CAMLlocal2(ans, plane);
  AVFrame *frame = Frame_val(_frame);
  int i;

  if (Bool_val(_make_writable)) {
    caml_release_runtime_system();
    int ret = av_frame_make_writable(frame);
    caml_acquire_runtime_system();

    if (ret < 0)
      ocaml_avutil_raise_error(ret);
  }

  caml_release_runtime_system();
  int nb_planes = av_pix_fmt_count_planes((enum AVPixelFormat)frame->format);
  caml_acquire_runtime_system();

  if (nb_planes < 0)
    ocaml_avutil_raise_error(nb_planes);

  ans = caml_alloc_tuple(nb_planes);

  for (i = 0; i < nb_planes; i++) {
    intnat out_size = frame->linesize[i] * frame->height;
    plane = caml_alloc_tuple(2);

    Store_field(plane, 0,
                caml_ba_alloc(CAML_BA_C_LAYOUT | CAML_BA_UINT8, 1,
                              frame->data[i], &out_size));
    Store_field(plane, 1, Val_int(frame->linesize[i]));
    Store_field(ans, i, plane);
  }

  CAMLreturn(ans);
}

/***** AVSubtitle *****/

void static finalize_subtitle(value v) {
  struct AVSubtitle *subtitle = Subtitle_val(v);
  avsubtitle_free(subtitle);
  free(subtitle);
}

static struct custom_operations subtitle_ops = {
    "ocaml_avsubtitle",  finalize_subtitle,        custom_compare_default,
    custom_hash_default, custom_serialize_default, custom_deserialize_default};

value value_of_subtitle(AVSubtitle *subtitle) {
  value ret;
  if (!subtitle)
    Fail("Empty subtitle");

  ret = caml_alloc_custom(&subtitle_ops, sizeof(AVSubtitle *), 0, 1);
  Subtitle_val(ret) = subtitle;

  return ret;
}

int subtitle_header_default(AVCodecContext *codec_context) { return 0; }

CAMLprim value ocaml_avutil_subtitle_create_frame(value _start_time,
                                                  value _end_time,
                                                  value _lines) {
  CAMLparam3(_start_time, _end_time, _lines);
  CAMLlocal1(ans);
  int64_t start_time = Int64_val(_start_time);
  int64_t end_time = Int64_val(_end_time);
  int nb_lines = Wosize_val(_lines);

  AVSubtitle *subtitle = (AVSubtitle *)calloc(1, sizeof(AVSubtitle));
  if (!subtitle)
    caml_raise_out_of_memory();

  ans = value_of_subtitle(subtitle);

  //  subtitle->start_display_time = (uint32_t)start_time;
  subtitle->end_display_time = (uint32_t)end_time;
  subtitle->pts = start_time;

  caml_release_runtime_system();
  subtitle->rects =
      (AVSubtitleRect **)av_malloc_array(nb_lines, sizeof(AVSubtitleRect *));
  caml_acquire_runtime_system();

  if (!subtitle->rects)
    caml_raise_out_of_memory();

  subtitle->num_rects = nb_lines;

  int i;
  for (i = 0; i < nb_lines; i++) {
    const char *text = String_val(Field(_lines, i));

    caml_release_runtime_system();
    subtitle->rects[i] = (AVSubtitleRect *)av_mallocz(sizeof(AVSubtitleRect));
    caml_acquire_runtime_system();

    if (!subtitle->rects[i])
      caml_raise_out_of_memory();

    subtitle->rects[i]->type = SUBTITLE_TEXT;
    subtitle->rects[i]->text = strdup(text);
    if (!subtitle->rects[i]->text)
      caml_raise_out_of_memory();

    //    subtitle->rects[i]->type = SUBTITLE_ASS;
    //    subtitle->rects[i]->ass = get_dialog(i + 1, 0, NULL, NULL, text);
    //    if( ! subtitle->rects[i]->ass) Fail( "Failed to allocate subtitle
    //    frame");
  }

  CAMLreturn(ans);
}

CAMLprim value ocaml_avutil_subtitle_to_lines(value _subtitle) {
  CAMLparam1(_subtitle);
  CAMLlocal2(ans, lines);
  struct AVSubtitle *subtitle = Subtitle_val(_subtitle);
  unsigned i, num_rects = subtitle->num_rects;

  lines = caml_alloc_tuple(num_rects);

  for (i = 0; i < num_rects; i++) {
    char *line = subtitle->rects[i]->text ? subtitle->rects[i]->text
                                          : subtitle->rects[i]->ass;
    Store_field(lines, i, caml_copy_string(line));
  }

  ans = caml_alloc_tuple(3);
  Store_field(ans, 0, caml_copy_int64((int64_t)subtitle->start_display_time));
  Store_field(ans, 1, caml_copy_int64((int64_t)subtitle->end_display_time));
  Store_field(ans, 2, lines);

  CAMLreturn(ans);
}

CAMLprim value ocaml_avutil_av_opt_next(value _cursor, value _class) {
  CAMLparam2(_cursor, _class);
  CAMLlocal4(_opt, _type, _tmp, _spec);

  const AVClass *_class_cursor;
  const struct AVOption *_opt_cursor;
  AVRational r;

  if (_cursor == Val_none) {
    _opt_cursor = (const struct AVOption *)NULL;
    _class_cursor = (const AVClass *)_class;
  } else {
    _opt_cursor = (const struct AVOption *)Field(Some_val(_cursor), 0);
    _class_cursor = (const AVClass *)Field(Some_val(_cursor), 1);
  }

  if (_class_cursor == NULL)
    CAMLreturn(Val_none);

  _opt_cursor = av_opt_next(&_class_cursor, _opt_cursor);

  if (_opt_cursor == NULL) {
    do {
      _class_cursor =
          av_opt_child_class_next((const AVClass *)_class, _class_cursor);

      if (_class_cursor == NULL)
        CAMLreturn(Val_none);

      _opt_cursor = av_opt_next(&_class_cursor, _opt_cursor);
    } while (_opt_cursor == NULL);
  }

  if (_opt_cursor == NULL)
    CAMLreturn(Val_none);

  _opt = caml_alloc_tuple(6);
  Store_field(_opt, 0, caml_copy_string(_opt_cursor->name));

  if (_opt_cursor->help == NULL || strlen(_opt_cursor->help) == 0)
    Store_field(_opt, 1, Val_none);
  else {
    _tmp = caml_alloc_small(1, 0);
    Store_field(_tmp, 0, caml_copy_string(_opt_cursor->help));
    Store_field(_opt, 1, _tmp);
  }

  _spec = caml_alloc_tuple(3);
  _tmp = caml_alloc_small(1, 0);
  Store_field(_spec, 0, Val_none);
  Store_field(_spec, 1, Val_none);
  Store_field(_spec, 2, Val_none);
  Store_field(_opt, 2, _spec);

  switch (_opt_cursor->type) {
  case AV_OPT_TYPE_CONST:
    _type = PVV_Constant;
    Store_field(_tmp, 0, (value)_opt_cursor);
    Store_field(_spec, 0, _tmp);
    break;
  case AV_OPT_TYPE_BOOL:
    _type = PVV_Bool;
    if (_opt_cursor->default_val.i64 >= 0) {
      Store_field(_tmp, 0, Val_bool(_opt_cursor->default_val.i64));
      Store_field(_spec, 0, _tmp);
    }
    break;
  case AV_OPT_TYPE_CHANNEL_LAYOUT:
    _type = PVV_Channel_layout;
    if (av_get_channel_name(_opt_cursor->default_val.i64)) {
      Store_field(_tmp, 0, Val_ChannelLayout(_opt_cursor->default_val.i64));
      Store_field(_spec, 0, _tmp);
    }
    break;
  case AV_OPT_TYPE_PIXEL_FMT:
    _type = PVV_Pixel_fmt;
    if (av_get_pix_fmt_name(_opt_cursor->default_val.i64)) {
      Store_field(_tmp, 0, Val_PixelFormat(_opt_cursor->default_val.i64));
      Store_field(_spec, 0, _tmp);
    }
    break;
  case AV_OPT_TYPE_SAMPLE_FMT:
    _type = PVV_Sample_fmt;
    if (av_get_sample_fmt_name(_opt_cursor->default_val.i64)) {
      Store_field(_tmp, 0, Val_SampleFormat(_opt_cursor->default_val.i64));
      Store_field(_spec, 0, _tmp);
    }
    break;
  case AV_OPT_TYPE_INT:
    _type = PVV_Int;

    Store_field(_tmp, 0, Val_int(_opt_cursor->default_val.i64));
    Store_field(_spec, 0, _tmp);

    _tmp = caml_alloc_small(1, 0);
    Store_field(_tmp, 0, Val_int(_opt_cursor->min));
    Store_field(_spec, 1, _tmp);

    _tmp = caml_alloc_small(1, 0);
    Store_field(_tmp, 0, Val_int(_opt_cursor->max));
    Store_field(_spec, 2, _tmp);
    break;

  case AV_OPT_TYPE_FLAGS:
    _type = PVV_Flags;
    goto int64_opt;
  case AV_OPT_TYPE_INT64:
    _type = PVV_Int64;
    goto int64_opt;
  case AV_OPT_TYPE_UINT64:
    _type = PVV_UInt64;
    goto int64_opt;
  case AV_OPT_TYPE_DURATION:
    _type = PVV_Duration;

  int64_opt:
    Store_field(_tmp, 0, caml_copy_int64(_opt_cursor->default_val.i64));
    Store_field(_spec, 0, _tmp);

    _tmp = caml_alloc_small(1, 0);

    if (_opt_cursor->min <= INT64_MIN)
      Store_field(_tmp, 0, caml_copy_int64(INT64_MIN));
    else if (_opt_cursor->min >= INT64_MAX)
      Store_field(_tmp, 0, caml_copy_int64(INT64_MAX));
    else
      Store_field(_tmp, 0, caml_copy_int64(_opt_cursor->min));

    Store_field(_spec, 1, _tmp);

    _tmp = caml_alloc_small(1, 0);

    if (_opt_cursor->max <= INT64_MIN)
      Store_field(_tmp, 0, caml_copy_int64(INT64_MIN));
    else if (_opt_cursor->max >= INT64_MAX)
      Store_field(_tmp, 0, caml_copy_int64(INT64_MAX));
    else
      Store_field(_tmp, 0, caml_copy_int64(_opt_cursor->max));

    Store_field(_spec, 2, _tmp);
    break;

  case AV_OPT_TYPE_DOUBLE:
    _type = PVV_Double;
    goto float_opt;
  case AV_OPT_TYPE_FLOAT:
    _type = PVV_Float;

  float_opt:
    Store_field(_tmp, 0, caml_copy_double(_opt_cursor->default_val.dbl));
    Store_field(_spec, 0, _tmp);

    _tmp = caml_alloc_small(1, 0);
    Store_field(_tmp, 0, caml_copy_double(_opt_cursor->min));
    Store_field(_spec, 1, _tmp);

    _tmp = caml_alloc_small(1, 0);
    Store_field(_tmp, 0, caml_copy_double(_opt_cursor->max));
    Store_field(_spec, 2, _tmp);
    break;

  case AV_OPT_TYPE_RATIONAL:
    _type = PVV_Rational;

    Store_field(_spec, 0, _tmp);
    r = av_d2q(_opt_cursor->default_val.dbl, INT_MAX);
    value_of_rational(&r, &_tmp);
    Store_field(Field(_spec, 0), 0, _tmp);

    Store_field(_spec, 1, caml_alloc_small(1, 0));
    r = av_d2q(_opt_cursor->min, INT_MAX);
    value_of_rational(&r, &_tmp);
    Store_field(Field(_spec, 1), 0, _tmp);

    Store_field(_spec, 2, caml_alloc_small(1, 0));
    r = av_d2q(_opt_cursor->max, INT_MAX);
    value_of_rational(&r, &_tmp);
    Store_field(Field(_spec, 2), 0, _tmp);
    break;

  case AV_OPT_TYPE_COLOR:
    _type = PVV_Color;
    goto string_opt;
  case AV_OPT_TYPE_DICT:
    _type = PVV_Dict;
    goto string_opt;
  case AV_OPT_TYPE_IMAGE_SIZE:
    _type = PVV_Image_size;
    goto string_opt;
  case AV_OPT_TYPE_VIDEO_RATE:
    _type = PVV_Video_rate;
    goto string_opt;
  case AV_OPT_TYPE_BINARY:
    _type = PVV_Binary;
    goto string_opt;
  case AV_OPT_TYPE_STRING:
    _type = PVV_String;

  string_opt:
    if (_opt_cursor->default_val.str) {
      Store_field(_tmp, 0, caml_copy_string(_opt_cursor->default_val.str));
      Store_field(_spec, 0, _tmp);
    }
    break;
  default:
    caml_failwith("Invalid option type!");
  }

  _tmp = caml_alloc_tuple(2);
  Store_field(_tmp, 0, _type);
  Store_field(_tmp, 1, _spec);
  Store_field(_opt, 2, _tmp);

  if (_opt_cursor->unit == NULL || strlen(_opt_cursor->unit) == 0)
    Store_field(_opt, 3, Val_none);
  else {
    _tmp = caml_alloc_small(1, 0);
    Store_field(_tmp, 0, caml_copy_string(_opt_cursor->unit));
    Store_field(_opt, 3, _tmp);
  }

  _tmp = caml_alloc_small(1, 0);
  Store_field(_tmp, 0, caml_alloc_tuple(2));
  Store_field(Field(_tmp, 0), 0, (value)_opt_cursor);
  Store_field(Field(_tmp, 0), 1, (value)_class_cursor);
  Store_field(_opt, 4, _tmp);

  _tmp = caml_alloc_small(1, 0);
  Store_field(_tmp, 0, _opt);

  CAMLreturn(_tmp);
}

CAMLprim value ocaml_avutil_avopt_default_int64(value _opt) {
  CAMLparam0();
  CAMLreturn(caml_copy_int64(((const AVOption *)_opt)->default_val.i64));
}

CAMLprim value ocaml_avutil_avopt_default_double(value _opt) {
  CAMLparam0();
  CAMLreturn(caml_copy_double(((const AVOption *)_opt)->default_val.dbl));
}

CAMLprim value ocaml_avutil_avopt_default_string(value _opt) {
  CAMLparam0();
  CAMLreturn(caml_copy_string(((const AVOption *)_opt)->default_val.str));
}
