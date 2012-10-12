#include <caml/mlvalues.h>
#include <caml/memory.h>
#include <caml/alloc.h>
#include <caml/fail.h>
#include <caml/callback.h>
#include <caml/custom.h>
#include <caml/bigarray.h>
#include <caml/threads.h>

#include <libavutil/pixfmt.h>

static const enum PixelFormat PIXEL_FORMATS[] = { PIX_FMT_YUV420P, PIX_FMT_YUYV422, PIX_FMT_RGB24, PIX_FMT_BGR24, PIX_FMT_YUV422P, PIX_FMT_YUV444P, PIX_FMT_RGBA };

int PixelFormat_val(value v)
{
  return PIXEL_FORMATS[Int_val(v)];
}
