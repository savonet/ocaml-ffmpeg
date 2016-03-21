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

CAMLprim value caml_avutil_bits_per_pixel(value pixel)
{
  CAMLparam1(pixel);
  enum AVPixelFormat p = PixelFormat_val(pixel);
  int ans;

  ans = av_get_bits_per_pixel(av_pix_fmt_desc_get(p));

  CAMLreturn(Val_int(ans));
}
