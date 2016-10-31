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

#include <avutil_stubs.h>

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
#define SECOND_FRACTIONS_LEN 4
static const int64_t SECOND_FRACTIONS[SECOND_FRACTIONS_LEN] = {
  1, 1000, 1000000, 1000000000
};

int64_t second_fractions_of_time_format(int time_format)
{
  return SECOND_FRACTIONS[time_format];
}


/**** Channel layout ****/

#define CHANNEL_LAYOUTS_LEN 28
static const uint64_t CHANNEL_LAYOUTS[CHANNEL_LAYOUTS_LEN] = {
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

uint64_t ChannelLayout_val(value v)
{
  return CHANNEL_LAYOUTS[Int_val(v)];
}

value Val_channelLayout(uint64_t cl)
{
  for (int i = 0; i < CHANNEL_LAYOUTS_LEN; i++) {
    if (cl == CHANNEL_LAYOUTS[i])
      return Val_int(i);
  }
  printf("error in channel layout : %llu\n", cl);
  return Val_int(0);
}


/**** Sample format ****/

#define SAMPLE_FORMATS_LEN 10
static const enum AVSampleFormat SAMPLE_FORMATS[SAMPLE_FORMATS_LEN] = {
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
  for (int i = 0; i < SAMPLE_FORMATS_LEN; i++) {
    if (sf == SAMPLE_FORMATS[i])
      return Val_int(i);
  }
  printf("error in sample format : %d\n", sf);
  return Val_int(0);
}

static const enum caml_ba_kind BIGARRAY_KINDS[SAMPLE_FORMATS_LEN] = {
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

enum caml_ba_kind bigarray_kind_of_Sample_format(int sf)
{
  return BIGARRAY_KINDS[sf];
}

enum caml_ba_kind bigarray_kind_of_Sample_format_val(value val)
{
  return BIGARRAY_KINDS[Int_val(val)];
}

enum caml_ba_kind bigarray_kind_of_AVSampleFormat(enum AVSampleFormat sf)
{
  for (int i = 0; i < SAMPLE_FORMATS_LEN; i++)
    {
      if (sf == SAMPLE_FORMATS[i])
	return BIGARRAY_KINDS[i];
    }
  return CAML_BA_KIND_MASK;
}

CAMLprim value ocaml_avutil_get_sample_fmt_name(value _sample_fmt) {
  CAMLparam1(_sample_fmt);
  CAMLlocal1(ans);

  const char *name = av_get_sample_fmt_name(SampleFormat_val(_sample_fmt));
  ans = caml_copy_string(name);

  CAMLreturn(ans);
}


/***** AVFrame *****/

static void finalize_frame(value v)
{
  struct AVFrame *frame = Frame_val(v);
  av_frame_free(&frame);
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

value value_of_frame(AVFrame *frame)
{
  if (!frame)
    Raise(EXN_FAILURE, "Empty frame");

  value ans = caml_alloc_custom(&frame_ops, sizeof(AVFrame*), 0, 1);
  Frame_val(ans) = frame;
  return ans;
}
