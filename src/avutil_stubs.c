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

#include "avutil_stubs.h"

char ocaml_av_error_msg[ERROR_MSG_SIZE + 1];
char ocaml_av_exn_msg[ERROR_MSG_SIZE + 1];

static const enum AVPixelFormat PIXEL_FORMATS[] = {
  AV_PIX_FMT_YUV420P,
  AV_PIX_FMT_YUYV422,
  AV_PIX_FMT_RGB24,
  AV_PIX_FMT_BGR24,
  AV_PIX_FMT_YUV422P,
  AV_PIX_FMT_YUV444P,
  AV_PIX_FMT_YUV410P,
  AV_PIX_FMT_YUV411P,
  AV_PIX_FMT_YUVJ422P,
  AV_PIX_FMT_YUVJ444P,
  AV_PIX_FMT_RGBA,
  AV_PIX_FMT_BGRA
};

enum AVPixelFormat PixelFormat_val(value v)
{
  return PIXEL_FORMATS[Int_val(v)];
}

CAMLprim value ocaml_avutil_bits_per_pixel(value pixel)
{
  CAMLparam1(pixel);
  enum AVPixelFormat p = PixelFormat_val(pixel);
  int ans;

  ans = av_get_bits_per_pixel(av_pix_fmt_desc_get(p));

  CAMLreturn(Val_int(ans));
}


/**** Time format ****/
static const int64_t SECOND_FRACTIONS[] = {
  1, 1000, 1000000, 1000000000
};
#define SECOND_FRACTIONS_LEN (sizeof(SECOND_FRACTIONS) / sizeof(int64_t))

int64_t second_fractions_of_time_format(int time_format)
{
  return SECOND_FRACTIONS[time_format];
}


/**** Channel layout ****/

#ifdef HAS_CHANNEL_LAYOUT
static const uint64_t CHANNEL_LAYOUTS[] = {
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
  //  AV_CH_LAYOUT_HEXADECAGONAL,
  AV_CH_LAYOUT_STEREO_DOWNMIX
};
#define CHANNEL_LAYOUTS_LEN (sizeof(CHANNEL_LAYOUTS) / sizeof(uint64_t))
#endif

uint64_t ChannelLayout_val(value v)
{
#ifndef HAS_CHANNEL_LAYOUT
  caml_failwith("Not implemented.");
#else
  return CHANNEL_LAYOUTS[Int_val(v)];
#endif
}

value Val_channelLayout(uint64_t cl)
{
#ifndef HAS_CHANNEL_LAYOUT
  caml_failwith("Not implemented.");
#else
  int i;
  for (i = 0; i < CHANNEL_LAYOUTS_LEN; i++) {
    if (cl == CHANNEL_LAYOUTS[i])
      return Val_int(i);
  }
  Raise(EXN_FAILURE, "Invalid channel layout : %llu", cl);
  return Val_int(0);
#endif
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

enum AVSampleFormat SampleFormat_val(value v)
{
  return SAMPLE_FORMATS[Int_val(v)];
}

enum AVSampleFormat AVSampleFormat_of_Sample_format(int i)
{
  return SAMPLE_FORMATS[i];
}

value Val_sampleFormat(enum AVSampleFormat sf)
{
  int i;
  for (i = 0; i < SAMPLE_FORMATS_LEN; i++) {
    if (sf == SAMPLE_FORMATS[i])
      return Val_int(i);
  }
  Raise(EXN_FAILURE, "Invalid sample format : %d", sf);
  return Val_int(0);
}

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


/***** AVFrame *****/

static void finalize_frame(value v)
{
#ifdef HAS_FRAME
  struct AVFrame *frame = Frame_val(v);
  av_frame_free(&frame);
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

CAMLprim value ocaml_avutil_video_frame_create(value _w, value _h, value _format)
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
  if(ret < 0) Raise(EXN_FAILURE, "Failed to alloc video frame buffer");

  value_of_frame(frame, &ans);
#endif
  CAMLreturn(ans);
}

CAMLprim value ocaml_avutil_video_frame_get(value _frame, value _line, value _x, value _y)
{
  CAMLparam1(_frame);
#ifndef HAS_FRAME
  caml_failwith("Not implemented.");
#else
  AVFrame *frame = Frame_val(_frame);
  int line = Int_val(_line);
  int x = Int_val(_x);
  int y = Int_val(_y);
#endif
  CAMLreturn(Val_int(frame->data[line][y * frame->linesize[line] + x]));
}

CAMLprim value ocaml_avutil_video_frame_set(value _frame, value _line, value _x, value _y, value _v)
{
  CAMLparam1(_frame);
#ifndef HAS_FRAME
  caml_failwith("Not implemented.");
#else
  AVFrame *frame = Frame_val(_frame);
  int line = Int_val(_line);
  int x = Int_val(_x);
  int y = Int_val(_y);

  frame->data[line][y * frame->linesize[line] + x] = Int_val(_v);
#endif
  CAMLreturn(Val_unit);
}
