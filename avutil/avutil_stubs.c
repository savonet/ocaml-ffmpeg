#include <assert.h>
#include <pthread.h>
#include <string.h>

#define CAML_NAME_SPACE 1

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
#include <libavutil/pixdesc.h>
#include <libavutil/pixfmt.h>
#include <libavutil/mem.h>

#include "avutil_stubs.h"
#include "channel_layout_stubs.h"
#include "hw_device_type_stubs.h"
#include "pixel_format_flag_stubs.h"
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

CAMLprim value ocaml_avutil_qp2lambda(value unit) {
  CAMLparam0();
  CAMLreturn(Val_int(FF_QP2LAMBDA));
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
  Store_field(*pvalue, 0, Val_int(rational->num));
  Store_field(*pvalue, 1, Val_int(rational->den));
}

value ocaml_avutil_av_d2q(value f) {
  CAMLparam1(f);
  CAMLlocal1(ret);

  const AVRational r = av_d2q(Double_val(f), INT_MAX);
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

static void finalize_channel_layout(value v) {
  AVChannelLayout *channel_layout = AVChannelLayout_val(v);
  av_channel_layout_uninit(channel_layout);
  av_free(channel_layout);
}

static struct custom_operations channel_layout_ops = {
    "ocaml_avchannel_layout", finalize_channel_layout,
    custom_compare_default,   custom_hash_default,
    custom_serialize_default, custom_deserialize_default};

AVChannelLayout *caml_avutil_alloc_custom_channel_layout(value *ret) {
  AVChannelLayout *ch_layout;
  int err;

  ch_layout = av_mallocz(sizeof(AVChannelLayout));

  if (!ch_layout)
    caml_raise_out_of_memory();

  *ret =
      caml_alloc_custom(&channel_layout_ops, sizeof(AVChannelLayout *), 0, 1);
  AVChannelLayout_val(*ret) = ch_layout;

  return ch_layout;
}

void value_of_channel_layout(value *ret,
                             const AVChannelLayout *channel_layout) {
  AVChannelLayout *ch_layout = caml_avutil_alloc_custom_channel_layout(ret);
  int err;

  err = av_channel_layout_copy(ch_layout, channel_layout);

  if (err) {
    av_free(ch_layout);
    ocaml_avutil_raise_error(err);
  }
}

#define AVChannelLayoutOpaque_val(v) (*(void ***)Data_custom_val(v))

static void finalize_opaque(value v) {
  void **opaque = AVChannelLayoutOpaque_val(v);
  av_free(opaque);
}

static struct custom_operations opaque_ops = {
    "ocaml_avchannel_layout_opaque", finalize_opaque,
    custom_compare_default,          custom_hash_default,
    custom_serialize_default,        custom_deserialize_default};

CAMLprim value ocaml_avutil_start_standard_iteration() {
  CAMLparam0();
  CAMLlocal1(ret);

  void **opaque = av_malloc(sizeof(void *));

  if (!opaque)
    caml_raise_out_of_memory();

  *opaque = NULL;

  ret = caml_alloc_custom(&opaque_ops, sizeof(void *), 0, 1);
  AVChannelLayoutOpaque_val(ret) = opaque;

  CAMLreturn(ret);
}

CAMLprim value ocaml_avutil_get_standard(value _opaque) {
  CAMLparam1(_opaque);
  CAMLlocal2(_ch_layout, ret);
  void **opaque = AVChannelLayoutOpaque_val(_opaque);
  const AVChannelLayout *channel_layout = av_channel_layout_standard(opaque);

  if (!channel_layout)
    CAMLreturn(Val_none);

  ret = caml_alloc_tuple(1);

  value_of_channel_layout(&_ch_layout, channel_layout);

  Store_field(ret, 0, _ch_layout);

  CAMLreturn(ret);
}

CAMLprim value ocaml_avutil_compare_channel_layout(value _layout1,
                                                   value _layout2) {
  CAMLparam2(_layout1, _layout2);
  int ret = av_channel_layout_compare(AVChannelLayout_val(_layout1),
                                      AVChannelLayout_val(_layout2));

  if (ret < 0)
    ocaml_avutil_raise_error(ret);

  CAMLreturn(Val_bool(!ret));
}

CAMLprim value ocaml_avutil_get_channel_native_id(value _channel_layout) {
  CAMLparam1(_channel_layout);
  CAMLlocal1(ans);

  AVChannelLayout *channel_layout = AVChannelLayout_val(_channel_layout);

  if (channel_layout->order != AV_CHANNEL_ORDER_NATIVE)
    CAMLreturn(Val_none);

  ans = caml_alloc_tuple(1);
  Store_field(ans, 0, caml_copy_int64(channel_layout->u.mask));
  CAMLreturn(ans);
}

CAMLprim value
ocaml_avutil_get_channel_layout_description(value _channel_layout) {
  CAMLparam1(_channel_layout);
  char buf[1024];
  AVChannelLayout *channel_layout = AVChannelLayout_val(_channel_layout);
  int err = av_channel_layout_describe(channel_layout, buf, sizeof(buf));

  if (err < 0)
    ocaml_avutil_raise_error(err);

  CAMLreturn(caml_copy_string(buf));
}

CAMLprim value
ocaml_avutil_get_channel_layout_nb_channels(value _channel_layout) {
  CAMLparam1(_channel_layout);
  AVChannelLayout *channel_layout = AVChannelLayout_val(_channel_layout);
  CAMLreturn(Val_int(channel_layout->nb_channels));
}

CAMLprim value ocaml_avutil_get_default_channel_layout(value _nb_channels) {
  CAMLparam0();
  CAMLlocal1(_ch_layout);
  AVChannelLayout *channel_layout =
      caml_avutil_alloc_custom_channel_layout(&_ch_layout);

  av_channel_layout_default(channel_layout, Int_val(_nb_channels));

  CAMLreturn(_ch_layout);
}

CAMLprim value ocaml_avutil_get_channel_layout(value _name) {
  CAMLparam1(_name);
  CAMLlocal1(_ch_layout);
  AVChannelLayout *channel_layout =
      caml_avutil_alloc_custom_channel_layout(&_ch_layout);

  int err = av_channel_layout_from_string(channel_layout, String_val(_name));

  if (err)
    ocaml_avutil_raise_error(err);

  CAMLreturn(_ch_layout);
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
  char *name = av_strndup(String_val(_name), caml_string_length(_name));
  if (!name)
    caml_raise_out_of_memory();

  enum AVSampleFormat ret = av_get_sample_fmt(name);

  free(name);

  if (ret == AV_SAMPLE_FMT_NONE)
    caml_raise_not_found();

  CAMLreturn(Val_SampleFormat(ret));
}

CAMLprim value ocaml_avutil_get_sample_fmt_name(value _sample_fmt) {
  CAMLparam1(_sample_fmt);
  CAMLlocal1(ans);
  enum AVSampleFormat sample_fmt = SampleFormat_val(_sample_fmt);

  if (sample_fmt == AV_SAMPLE_FMT_NONE)
    CAMLreturn(Val_none);

  const char *name = av_get_sample_fmt_name(SampleFormat_val(_sample_fmt));

  if (!name)
    CAMLreturn(Val_none);

  ans = caml_alloc_tuple(1);
  Store_field(ans, 0, caml_copy_string(name));

  CAMLreturn(ans);
}

CAMLprim value ocaml_avutil_get_sample_fmt_id(value _sample_fmt) {
  CAMLparam1(_sample_fmt);
  CAMLreturn(Val_int(SampleFormat_val(_sample_fmt)));
}

CAMLprim value ocaml_avutil_find_sample_fmt_from_id(value _id) {
  CAMLparam0();
  value ret = Val_SampleFormat(Int_val(_id));

  CAMLreturn(ret);
}

/***** AVPixelFormat *****/
CAMLprim value ocaml_avutil_pixelformat_descriptor(value pixel) {
  CAMLparam1(pixel);
  CAMLlocal4(ret, tmp1, tmp2, cons);
  enum AVPixelFormat p = PixelFormat_val(pixel);
  const AVPixFmtDescriptor *pixdesc = av_pix_fmt_desc_get(p);
  AVComponentDescriptor comp_desc;
  int i, n;

  if (!pixdesc)
    caml_raise_not_found();

  ret = caml_alloc_tuple(8);
  Store_field(ret, 0, caml_copy_string(pixdesc->name));
  Store_field(ret, 1, Val_int(pixdesc->nb_components));
  Store_field(ret, 2, Val_int(pixdesc->log2_chroma_w));
  Store_field(ret, 3, Val_int(pixdesc->log2_chroma_h));

  n = 0;
  for (i = 0; i < AV_PIX_FMT_FLAG_T_TAB_LEN; i++) {
    if (pixdesc->flags & AV_PIX_FMT_FLAG_T_TAB[i][1])
      n++;
  }

  if (n == 0)
    Store_field(ret, 4, Val_int(0));
  else {
    cons = Val_int(0);
    for (i = 0; i < AV_PIX_FMT_FLAG_T_TAB_LEN; i++) {
      if (pixdesc->flags & AV_PIX_FMT_FLAG_T_TAB[i][1]) {
        tmp1 = caml_alloc(2, 0);
        Store_field(tmp1, 0, AV_PIX_FMT_FLAG_T_TAB[i][0]);
        Store_field(tmp1, 1, cons);
        cons = tmp1;
      }
    }
    Store_field(ret, 4, tmp1);
  }

  cons = Val_int(0);
  for (i = 3; i >= 0; i--) {
    comp_desc = pixdesc->comp[i];
    tmp2 = caml_alloc_tuple(5);
    Store_field(tmp2, 0, comp_desc.plane);
    Store_field(tmp2, 1, comp_desc.step);
    Store_field(tmp2, 2, comp_desc.offset);
    Store_field(tmp2, 3, comp_desc.shift);
    Store_field(tmp2, 4, comp_desc.depth);

    tmp1 = caml_alloc(2, 0);
    Store_field(tmp1, 0, tmp2);
    Store_field(tmp1, 1, cons);
    cons = tmp1;
  }
  Store_field(ret, 5, tmp1);

  if (pixdesc->alias) {
    tmp1 = caml_alloc_tuple(1);
    Store_field(tmp1, 0, caml_copy_string(pixdesc->alias));
    Store_field(ret, 6, tmp1);
  } else
    Store_field(ret, 6, Val_none);
  Store_field(ret, 7, value_of_avpixfmtdescriptor(tmp1, pixdesc));

  CAMLreturn(ret);
}

CAMLprim value ocaml_avutil_pixelformat_bits_per_pixel(value d) {
  CAMLparam0();
  const AVPixFmtDescriptor *pixdesc = AvPixFmtDescriptor_val(Field(d, 7));

  CAMLreturn(Val_int(av_get_bits_per_pixel(pixdesc)));
}

CAMLprim value ocaml_avutil_pixelformat_planes(value pixel) {
  CAMLparam1(pixel);
  enum AVPixelFormat p = PixelFormat_val(pixel);

  CAMLreturn(Val_int(av_pix_fmt_count_planes(p)));
}

CAMLprim value ocaml_avutil_get_pixel_fmt_id(value _pixel_fmt) {
  CAMLparam1(_pixel_fmt);
  CAMLreturn(Val_int(PixelFormat_val(_pixel_fmt)));
}

CAMLprim value ocaml_avutil_find_pixel_fmt_from_id(value _id) {
  CAMLparam0();
  value ret = Val_PixelFormat(Int_val(_id));

  CAMLreturn(ret);
}

CAMLprim value ocaml_avutil_pixelformat_to_string(value pixel) {
  CAMLparam1(pixel);
  CAMLlocal1(ret);
  enum AVPixelFormat p = PixelFormat_val(pixel);

  if (p == AV_PIX_FMT_NONE)
    CAMLreturn(Val_none);

  const char *name = av_get_pix_fmt_name(p);

  if (!name)
    CAMLreturn(Val_none);

  ret = caml_alloc_tuple(1);
  Store_field(ret, 0, caml_copy_string(name));
  CAMLreturn(ret);
}

CAMLprim value ocaml_avutil_pixelformat_of_string(value name) {
  CAMLparam1(name);

  enum AVPixelFormat p = av_get_pix_fmt(String_val(name));

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

  int size = 0;
  int n = 0;
  while (n < AV_NUM_DATA_POINTERS && frame->buf[n] != NULL) {
    size += frame->buf[n]->size;
    n++;
  }

  ret = caml_alloc_custom_mem(&frame_ops, sizeof(AVFrame *), size);
  Frame_val(ret) = frame;

  return ret;
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

  frame->best_effort_timestamp = frame->pts;

  CAMLreturn(Val_unit);
}

CAMLprim value ocaml_avutil_frame_duration(value _frame) {
  CAMLparam1(_frame);
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(57, 30, 100)
  CAMLlocal1(ret);
  AVFrame *frame = Frame_val(_frame);

  if (frame->duration == 0)
    CAMLreturn(Val_none);

  ret = caml_alloc_tuple(1);
  Store_field(ret, 0, caml_copy_int64(frame->duration));

  CAMLreturn(ret);
#else
  CAMLreturn(Val_none);
#endif
}

CAMLprim value ocaml_avutil_frame_set_duration(value _frame, value _duration) {
  CAMLparam2(_frame, _duration);
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(57, 30, 100)
  AVFrame *frame = Frame_val(_frame);

  if (_duration == Val_none)
    frame->duration = 0;
  else
    frame->duration = Int64_val(Field(_duration, 0));
#endif

  CAMLreturn(Val_unit);
}

CAMLprim value ocaml_avutil_frame_pkt_dts(value _frame) {
  CAMLparam1(_frame);
  CAMLlocal1(ret);
  AVFrame *frame = Frame_val(_frame);

  if (frame->pkt_dts == AV_NOPTS_VALUE)
    CAMLreturn(Val_none);

  ret = caml_alloc_tuple(1);
  Store_field(ret, 0, caml_copy_int64(frame->pkt_dts));

  CAMLreturn(ret);
}

CAMLprim value ocaml_avutil_frame_set_pkt_dts(value _frame, value _dts) {
  CAMLparam2(_frame, _dts);
  AVFrame *frame = Frame_val(_frame);

  if (_dts == Val_none)
    frame->pkt_dts = AV_NOPTS_VALUE;
  else
    frame->pkt_dts = Int64_val(Field(_dts, 0));

  CAMLreturn(Val_unit);
}

CAMLprim value ocaml_avutil_frame_metadata(value _frame) {
  CAMLparam1(_frame);
  CAMLlocal4(ans, key, val, pair);
  AVFrame *frame = Frame_val(_frame);
  AVDictionary *metadata = frame->metadata;
  AVDictionaryEntry *entry = NULL;
  int count = av_dict_count(metadata);
  int i;

  ans = caml_alloc_tuple(count);

  for (i = 0; i < count; i++) {
    pair = caml_alloc_tuple(2);
    entry = av_dict_get(metadata, "", entry, AV_DICT_IGNORE_SUFFIX);
    Store_field(pair, 0, caml_copy_string(entry->key));
    Store_field(pair, 1, caml_copy_string(entry->value));
    Store_field(ans, i, pair);
  }

  CAMLreturn(ans);
}

CAMLprim value ocaml_avutil_frame_set_metadata(value _frame, value _metadata) {
  CAMLparam2(_frame, _metadata);
  AVFrame *frame = Frame_val(_frame);
  AVDictionary *metadata = NULL;
  AVDictionaryEntry *entry = NULL;
  int i, ret;

  for (i = 0; i < Wosize_val(_metadata); i++) {
    ret = av_dict_set(&metadata, String_val(Field(Field(_metadata, i), 0)),
                      String_val(Field(Field(_metadata, i), 1)), 0);
    if (ret < 0)
      ocaml_avutil_raise_error(ret);
  }

  if (frame->metadata) {
    av_dict_free(&frame->metadata);
  }
  frame->metadata = metadata;

  CAMLreturn(Val_unit);
}

CAMLprim value ocaml_avutil_frame_best_effort_timestamp(value _frame) {
  CAMLparam1(_frame);
  CAMLlocal1(ret);
  AVFrame *frame = Frame_val(_frame);

  if (frame->best_effort_timestamp == AV_NOPTS_VALUE)
    CAMLreturn(Val_none);

  ret = caml_alloc_tuple(1);
  Store_field(ret, 0, caml_copy_int64(frame->best_effort_timestamp));

  CAMLreturn(ret);
}

CAMLprim value ocaml_avutil_frame_copy(value _src, value _dst) {
  CAMLparam2(_src, _dst);
  AVFrame *src = Frame_val(_src);
  AVFrame *dst = Frame_val(_dst);
  int ret;

  ret = av_frame_copy(dst, src);

  if (ret < 0)
    ocaml_avutil_raise_error(ret);

  CAMLreturn(Val_unit);
}

CAMLprim value ocaml_avutil_video_create_frame(value _w, value _h,
                                               value _format) {
  CAMLparam1(_format);
  AVFrame *frame = av_frame_alloc();
  if (!frame)
    caml_raise_out_of_memory();

  frame->format = PixelFormat_val(_format);
  frame->width = Int_val(_w);
  frame->height = Int_val(_h);

  int ret = av_frame_get_buffer(frame, 32);

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
  AVChannelLayout *channel_layout = AVChannelLayout_val(_channel_layout);
  int sample_rate = Int_val(_samplerate);
  int nb_samples = Int_val(_samples);
  int ret;

  AVFrame *frame = av_frame_alloc();

  if (!frame)
    caml_raise_out_of_memory();

  frame->format = sample_fmt;

  ret = av_channel_layout_copy(&frame->ch_layout, channel_layout);
  if (ret < 0) {
    av_frame_free(&frame);
    ocaml_avutil_raise_error(ret);
  }

  frame->sample_rate = sample_rate;
  frame->nb_samples = nb_samples;

  ret = av_frame_get_buffer(frame, 0);

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

  CAMLreturn(Val_int(frame->ch_layout.nb_channels));
}

CAMLprim value ocaml_avutil_audio_frame_get_channel_layout(value _frame) {
  CAMLparam1(_frame);
  CAMLlocal1(_ch_layout);
  AVFrame *frame = Frame_val(_frame);

  value_of_channel_layout(&_ch_layout, &frame->ch_layout);

  CAMLreturn(_ch_layout);
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
  int channels = dst->ch_layout.nb_channels;
  int planes = planar ? channels : 1;
  int i;

  if (src->nb_samples < src_ofs + len || dst->nb_samples < dst_ofs + len ||
      av_channel_layout_compare(&dst->ch_layout, &src->ch_layout))
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

CAMLprim value ocaml_avutil_video_frame_get_pixel_aspect(value _frame) {
  CAMLparam1(_frame);
  CAMLlocal2(ret, ans);
  AVFrame *frame = Frame_val(_frame);
  const AVRational pixel_aspect = frame->sample_aspect_ratio;

  if (pixel_aspect.num == 0)
    CAMLreturn(Val_none);

  value_of_rational(&pixel_aspect, &ans);

  ret = caml_alloc_tuple(1);
  Store_field(ret, 0, ans);

  CAMLreturn(ret);
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
    int ret = av_frame_make_writable(frame);

    if (ret < 0)
      ocaml_avutil_raise_error(ret);
  }

  int nb_planes = av_pix_fmt_count_planes((enum AVPixelFormat)frame->format);

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

  subtitle->rects =
      (AVSubtitleRect **)av_malloc_array(nb_lines, sizeof(AVSubtitleRect *));

  if (!subtitle->rects)
    caml_raise_out_of_memory();

  subtitle->num_rects = nb_lines;

  int i;
  for (i = 0; i < nb_lines; i++) {
    const char *text = String_val(Field(_lines, i));

    subtitle->rects[i] = (AVSubtitleRect *)av_mallocz(sizeof(AVSubtitleRect));

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

CAMLprim value ocaml_avutil_get_opt(value _type, value search_children,
                                    value name, value obj) {
  CAMLparam2(name, obj);
  CAMLlocal2(ret, tmp);

  uint8_t *str;
  int64_t err, i, search_flags = 0;
  double d;
  AVRational r;
  int w_out, h_out;
  AVChannelLayout *channel_layout;
  enum AVPixelFormat pf;
  enum AVSampleFormat sf;
  AVDictionary *dict = NULL;
  AVDictionaryEntry *dict_entry = NULL;
  int dict_length, dict_pos;

  if (Bool_val(search_children))
    search_flags = AV_OPT_SEARCH_CHILDREN;

  switch (_type) {
  case PVV_String:
    err = av_opt_get(AvObj_val(obj), (const char *)String_val(name),
                     search_flags, &str);
    if (err < 0)
      ocaml_avutil_raise_error(err);

    ret = caml_copy_string((char *)str);
    av_free(str);

    CAMLreturn(ret);
    break;

  case PVV_Int:
    err = av_opt_get_int((void *)obj, (const char *)String_val(name),
                         search_flags, &i);
    if (err < 0)
      ocaml_avutil_raise_error(err);

    CAMLreturn(Val_int(i));
    break;

  case PVV_Int64:
    err = av_opt_get_int((void *)obj, (const char *)String_val(name),
                         search_flags, &i);
    if (err < 0)
      ocaml_avutil_raise_error(err);

    CAMLreturn(caml_copy_int64(i));
    break;

  case PVV_Float:
    err = av_opt_get_double((void *)obj, (const char *)String_val(name),
                            search_flags, &d);
    if (err < 0)
      ocaml_avutil_raise_error(err);

    CAMLreturn(caml_copy_double(d));
    break;

  case PVV_Rational:
    err = av_opt_get_q((void *)obj, (const char *)String_val(name),
                       search_flags, &r);
    if (err < 0)
      ocaml_avutil_raise_error(err);

    value_of_rational(&r, &ret);

    CAMLreturn(ret);
    break;

  case PVV_Image_size:
    err = av_opt_get_image_size((void *)obj, (const char *)String_val(name),
                                search_flags, &w_out, &h_out);
    if (err < 0)
      ocaml_avutil_raise_error(err);

    ret = caml_alloc_tuple(2);
    Store_field(ret, 0, Val_int(w_out));
    Store_field(ret, 1, Val_int(h_out));

    CAMLreturn(ret);
    break;

  case PVV_Pixel_fmt:
    err = av_opt_get_pixel_fmt((void *)obj, (const char *)String_val(name),
                               search_flags, &pf);
    if (err < 0)
      ocaml_avutil_raise_error(err);

    CAMLreturn(Val_PixelFormat(pf));
    break;

  case PVV_Sample_fmt:
    err = av_opt_get_sample_fmt((void *)obj, (const char *)String_val(name),
                                search_flags, &sf);
    if (err < 0)
      ocaml_avutil_raise_error(err);

    CAMLreturn(Val_SampleFormat(sf));
    break;

  case PVV_Video_rate:
    err = av_opt_get_video_rate((void *)obj, (const char *)String_val(name),
                                search_flags, &r);
    if (err < 0)
      ocaml_avutil_raise_error(err);

    value_of_rational(&r, &ret);

    CAMLreturn(ret);
    break;

  case PVV_Channel_layout:
    channel_layout = caml_avutil_alloc_custom_channel_layout(&ret);
    err = av_opt_get_chlayout((void *)obj, (const char *)String_val(name),
                              search_flags, channel_layout);
    if (err < 0)
      ocaml_avutil_raise_error(err);

    CAMLreturn(ret);
    break;

  case PVV_Dict:
    err = av_opt_get_dict_val((void *)obj, (const char *)String_val(name),
                              search_flags, &dict);
    if (err < 0)
      ocaml_avutil_raise_error(err);

    dict_length = av_dict_count(dict);
    ret = caml_alloc_tuple(dict_length);

    for (dict_pos = 0; dict_pos < dict_length; dict_pos++) {
      dict_entry = av_dict_get(dict, "", dict_entry, AV_DICT_IGNORE_SUFFIX);
      tmp = caml_alloc_tuple(2);
      Store_field(tmp, 0, caml_copy_string(dict_entry->key));
      Store_field(tmp, 1, caml_copy_string(dict_entry->value));
      Store_field(ret, dict_pos, tmp);
    }

    av_dict_free(&dict);

    CAMLreturn(ret);
    break;

  default:
    caml_failwith("Invalid option type!");
  }
}

CAMLprim value ocaml_avutil_av_opt_iter(value _cursor, value _class) {
  CAMLparam2(_cursor, _class);
  CAMLlocal5(_opt, _type, _tmp, _spec, _ch_layout);
  int unimplement_option = 0;
  AVChannelLayout *channel_layout;

  const AVClass *class;
  const struct AVOption *option;
#if LIBAVUTIL_VERSION_INT < AV_VERSION_INT(56, 53, 100)
  const struct AVOption *cursor;
#else
  void *cursor;
#endif
  AVRational r;

  if (_cursor == Val_none) {
    cursor = NULL;
    option = NULL;
    class = AvClass_val(_class);
  } else {
#if LIBAVUTIL_VERSION_INT < AV_VERSION_INT(56, 53, 100)
    cursor = AvOptions_val(Field(Some_val(_cursor), 0));
    option = cursor;
#else
    cursor = AvObj_val(Field(Field(Some_val(_cursor), 0), 0));
    option = AvOptions_val(Field(Field(Some_val(_cursor), 0), 1));
#endif
    class = AvClass_val(Field(Some_val(_cursor), 1));
  }

  if (class == NULL)
    CAMLreturn(Val_none);

  option = av_opt_next(&class, option);

  if (option == NULL) {
    do {
      class =
#if LIBAVUTIL_VERSION_INT < AV_VERSION_INT(56, 53, 100)
          av_opt_child_class_next(AvClass_val(_class), class);
#else
          av_opt_child_class_iterate(AvClass_val(_class), &cursor);
#endif

      if (class == NULL)
        CAMLreturn(Val_none);

      option = av_opt_next(&class, option);
    } while (option == NULL);
  }

  _opt = caml_alloc_tuple(6);
  Store_field(_opt, 0, caml_copy_string(option->name));

  if (option->help == NULL || strlen(option->help) == 0)
    Store_field(_opt, 1, Val_none);
  else {
    _tmp = caml_alloc_tuple(1);
    Store_field(_tmp, 0, caml_copy_string(option->help));
    Store_field(_opt, 1, _tmp);
  }

  _spec = caml_alloc_tuple(3);
  _tmp = caml_alloc_tuple(1);
  Store_field(_spec, 0, Val_none);
  Store_field(_spec, 1, Val_none);
  Store_field(_spec, 2, Val_none);

  switch (option->type) {
  case AV_OPT_TYPE_CONST:
    _type = PVV_Constant;
    Store_field(_tmp, 0, value_of_avoptions(_cursor, option));
    Store_field(_spec, 0, _tmp);
    break;
  case AV_OPT_TYPE_BOOL:
    _type = PVV_Bool;
    if (option->default_val.i64 >= 0) {
      Store_field(_tmp, 0, Val_bool(option->default_val.i64));
      Store_field(_spec, 0, _tmp);
    }
    break;
  case AV_OPT_TYPE_CHLAYOUT:
    _type = PVV_Channel_layout;
    channel_layout = caml_avutil_alloc_custom_channel_layout(&_ch_layout);
    if (option->default_val.str &&
        av_channel_layout_from_string(channel_layout,
                                      option->default_val.str)) {
      Store_field(_tmp, 0, _ch_layout);
      Store_field(_spec, 0, _tmp);
    }
    break;
  case AV_OPT_TYPE_PIXEL_FMT:
    _type = PVV_Pixel_fmt;
    if (av_get_pix_fmt_name(option->default_val.i64)) {
      Store_field(_tmp, 0, Val_PixelFormat(option->default_val.i64));
      Store_field(_spec, 0, _tmp);
    }
    break;
  case AV_OPT_TYPE_SAMPLE_FMT:
    _type = PVV_Sample_fmt;
    if (av_get_sample_fmt_name(option->default_val.i64)) {
      Store_field(_tmp, 0, Val_SampleFormat(option->default_val.i64));
      Store_field(_spec, 0, _tmp);
    }
    break;
  case AV_OPT_TYPE_INT:
    _type = PVV_Int;

    Store_field(_tmp, 0, Val_int(option->default_val.i64));
    Store_field(_spec, 0, _tmp);

    _tmp = caml_alloc_tuple(1);
    Store_field(_tmp, 0, Val_int((int)option->min));
    Store_field(_spec, 1, _tmp);

    _tmp = caml_alloc_tuple(1);
    Store_field(_tmp, 0, Val_int((int)option->max));
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
    Store_field(_tmp, 0, caml_copy_int64(option->default_val.i64));
    Store_field(_spec, 0, _tmp);

    _tmp = caml_alloc_tuple(1);

    if (option->min <= INT64_MIN)
      Store_field(_tmp, 0, caml_copy_int64(INT64_MIN));
    else if (option->min >= INT64_MAX)
      Store_field(_tmp, 0, caml_copy_int64(INT64_MAX));
    else
      Store_field(_tmp, 0, caml_copy_int64((int64_t)option->min));

    Store_field(_spec, 1, _tmp);

    _tmp = caml_alloc_tuple(1);

    if (option->max <= INT64_MIN)
      Store_field(_tmp, 0, caml_copy_int64(INT64_MIN));
    else if (option->max >= INT64_MAX)
      Store_field(_tmp, 0, caml_copy_int64(INT64_MAX));
    else
      Store_field(_tmp, 0, caml_copy_int64((int64_t)option->max));

    Store_field(_spec, 2, _tmp);
    break;

  case AV_OPT_TYPE_DOUBLE:
    _type = PVV_Double;
    goto float_opt;
  case AV_OPT_TYPE_FLOAT:
    _type = PVV_Float;

  float_opt:
    Store_field(_tmp, 0, caml_copy_double(option->default_val.dbl));
    Store_field(_spec, 0, _tmp);

    _tmp = caml_alloc_tuple(1);
    Store_field(_tmp, 0, caml_copy_double(option->min));
    Store_field(_spec, 1, _tmp);

    _tmp = caml_alloc_tuple(1);
    Store_field(_tmp, 0, caml_copy_double(option->max));
    Store_field(_spec, 2, _tmp);
    break;

  case AV_OPT_TYPE_RATIONAL:
    _type = PVV_Rational;

    Store_field(_spec, 0, _tmp);
    r = av_d2q(option->default_val.dbl, INT_MAX);
    value_of_rational(&r, &_tmp);
    Store_field(Field(_spec, 0), 0, _tmp);

    Store_field(_spec, 1, caml_alloc_tuple(1));
    r = av_d2q(option->min, INT_MAX);
    value_of_rational(&r, &_tmp);
    Store_field(Field(_spec, 1), 0, _tmp);

    Store_field(_spec, 2, caml_alloc_tuple(1));
    r = av_d2q(option->max, INT_MAX);
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
    if (option->default_val.str) {
      Store_field(_tmp, 0, caml_copy_string(option->default_val.str));
      Store_field(_spec, 0, _tmp);
    }
    break;
  default:
    unimplement_option = 1;
  }

  _tmp = caml_alloc_tuple(2);
  Store_field(_tmp, 0, _type);
  Store_field(_tmp, 1, _spec);
  Store_field(_opt, 2, _tmp);

  Store_field(_opt, 3, Val_int(option->flags));

  if (option->unit == NULL || strlen(option->unit) == 0)
    Store_field(_opt, 4, Val_none);
  else {
    _tmp = caml_alloc_tuple(1);
    Store_field(_tmp, 0, caml_copy_string(option->unit));
    Store_field(_opt, 4, _tmp);
  }

  _tmp = caml_alloc_tuple(1);
  Store_field(_tmp, 0, caml_alloc_tuple(2));
#if LIBAVUTIL_VERSION_INT < AV_VERSION_INT(56, 53, 100)
  Store_field(Field(_tmp, 0), 0, value_of_avoptions(_cursor, option));
#else
  Store_field(Field(_tmp, 0), 0, caml_alloc_tuple(2));
  Store_field(Field(Field(_tmp, 0), 0), 0, value_of_avobj(_cursor, cursor));
  Store_field(Field(Field(_tmp, 0), 0), 1, value_of_avoptions(_cursor, option));
#endif
  Store_field(Field(_tmp, 0), 1, value_of_avclass(_class, class));
  Store_field(_opt, 5, _tmp);

  if (unimplement_option)
    caml_raise_with_arg(*caml_named_value("av_opt_iter_not_implemented"),
                        Field(_opt, 5));

  _tmp = caml_alloc_tuple(1);
  Store_field(_tmp, 0, _opt);

  CAMLreturn(_tmp);
}

CAMLprim value ocaml_avutil_avopt_default_int64(value _opt) {
  CAMLparam0();
  CAMLreturn(caml_copy_int64(AvOptions_val(_opt)->default_val.i64));
}

CAMLprim value ocaml_avutil_avopt_default_double(value _opt) {
  CAMLparam0();
  CAMLreturn(caml_copy_double(AvOptions_val(_opt)->default_val.dbl));
}

CAMLprim value ocaml_avutil_avopt_default_string(value _opt) {
  CAMLparam0();
  CAMLreturn(caml_copy_string(AvOptions_val(_opt)->default_val.str));
}

CAMLprim value ocaml_avutil_av_opt_int_of_flag(value _flag) {
  CAMLparam0();

  switch (_flag) {
  case PVV_Encoding_param:
    CAMLreturn(Val_int(AV_OPT_FLAG_ENCODING_PARAM));
  case PVV_Decoding_param:
    CAMLreturn(Val_int(AV_OPT_FLAG_DECODING_PARAM));
  case PVV_Audio_param:
    CAMLreturn(Val_int(AV_OPT_FLAG_AUDIO_PARAM));
  case PVV_Video_param:
    CAMLreturn(Val_int(AV_OPT_FLAG_VIDEO_PARAM));
  case PVV_Subtitle_param:
    CAMLreturn(Val_int(AV_OPT_FLAG_SUBTITLE_PARAM));
  case PVV_Export:
    CAMLreturn(Val_int(AV_OPT_FLAG_EXPORT));
  case PVV_Readonly:
    CAMLreturn(Val_int(AV_OPT_FLAG_READONLY));
  case PVV_Bsf_param:
#ifdef AV_OPT_FLAG_BSF_PARAM
    CAMLreturn(Val_int(AV_OPT_FLAG_BSF_PARAM));
#else
    CAMLreturn(Val_int(0));
#endif
  case PVV_Filtering_param:
    CAMLreturn(Val_int(AV_OPT_FLAG_FILTERING_PARAM));
  case PVV_Deprecated:
#ifdef AV_OPT_FLAG_DEPRECATED
    CAMLreturn(Val_int(AV_OPT_FLAG_DEPRECATED));
#else
    CAMLreturn(Val_int(0));
#endif
  case PVV_Child_consts:
#ifdef AV_OPT_FLAG_AV_OPT_FLAG_CHILD_CONSTS
    CAMLreturn(Val_int(AV_OPT_FLAG_CHILD_CONSTS));
#else
    CAMLreturn(Val_int(0));
#endif
  case PVV_Runtime_param:
#ifdef AV_OPT_FLAG_RUNTIME_PARAM
    CAMLreturn(Val_int(AV_OPT_FLAG_RUNTIME_PARAM));
#else
    CAMLreturn(Val_int(0));
#endif
  default:
    caml_failwith("Invalid option flag!");
  }
}

static void finalize_buffer_ref(value v) { av_buffer_unref(&BufferRef_val(v)); }

static struct custom_operations buffer_ref_ops = {
    "ocaml_avutil_buffer_ref", finalize_buffer_ref,
    custom_compare_default,    custom_hash_default,
    custom_serialize_default,  custom_deserialize_default};

CAMLprim value ocaml_avutil_create_device_context(value _device_type,
                                                  value _name, value _opts) {
  CAMLparam2(_name, _opts);
  CAMLlocal3(ret, ans, unused);
  AVBufferRef *hw_device_ctx = NULL;
  AVDictionary *options = NULL;
  const char *name;
  char *key, *val;
  int len = Wosize_val(_opts);
  int i, err, count;

  if (caml_string_length(_name) > 0) {
    name = String_val(_name);
  } else {
    name = NULL;
  }

  for (i = 0; i < len; i++) {
    // Dictionaries copy key/values by default!
    key = (char *)Bytes_val(Field(Field(_opts, i), 0));
    val = (char *)Bytes_val(Field(Field(_opts, i), 1));
    err = av_dict_set(&options, key, val, 0);
    if (err < 0) {
      av_dict_free(&options);
      ocaml_avutil_raise_error(err);
    }
  }

  caml_release_runtime_system();
  err = av_hwdevice_ctx_create(&hw_device_ctx, HwDeviceType_val(_device_type),
                               name, options, 0);
  caml_acquire_runtime_system();

  if (err < 0) {
    char errbuf[AV_ERROR_MAX_STRING_SIZE] = "";
    printf(
        "failed with error: %s\n",
        av_make_error_string(errbuf, AV_ERROR_MAX_STRING_SIZE, AVERROR(err)));
    fflush(stdout);
    av_dict_free(&options);
    ocaml_avutil_raise_error(err);
  }

  // Return unused keys
  count = av_dict_count(options);

  unused = caml_alloc_tuple(count);
  AVDictionaryEntry *entry = NULL;
  for (i = 0; i < count; i++) {
    entry = av_dict_get(options, "", entry, AV_DICT_IGNORE_SUFFIX);
    Store_field(unused, i, caml_copy_string(entry->key));
  }

  av_dict_free(&options);

  ans = caml_alloc_custom(&buffer_ref_ops, sizeof(AVBufferRef *), 0, 1);
  BufferRef_val(ans) = hw_device_ctx;

  ret = caml_alloc_tuple(2);
  Store_field(ret, 0, ans);
  Store_field(ret, 1, unused);

  CAMLreturn(ret);
}

CAMLprim value ocaml_avutil_create_frame_context(value _width, value _height,
                                                 value _src_pixel_format,
                                                 value _dst_pixel_format,
                                                 value _device_ctx) {
  CAMLparam1(_device_ctx);
  CAMLlocal1(ans);
  AVBufferRef *hw_frames_ref;
  AVHWFramesContext *frames_ctx = NULL;
  int ret;

  hw_frames_ref = av_hwframe_ctx_alloc(BufferRef_val(_device_ctx));

  if (!hw_frames_ref)
    caml_raise_out_of_memory();

  frames_ctx = (AVHWFramesContext *)(hw_frames_ref->data);
  frames_ctx->format = PixelFormat_val(_dst_pixel_format);
  frames_ctx->sw_format = PixelFormat_val(_src_pixel_format);
  frames_ctx->width = Int_val(_width);
  frames_ctx->height = Int_val(_height);

  caml_release_runtime_system();
  ret = av_hwframe_ctx_init(hw_frames_ref);
  caml_acquire_runtime_system();

  if (ret < 0) {
    av_buffer_unref(&hw_frames_ref);
    ocaml_avutil_raise_error(ret);
  }

  ans = caml_alloc_custom(&buffer_ref_ops, sizeof(AVBufferRef *), 0, 1);
  BufferRef_val(ans) = hw_frames_ref;

  CAMLreturn(ans);
}
