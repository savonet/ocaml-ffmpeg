#include <caml/mlvalues.h>
#include <caml/memory.h>
#include <caml/alloc.h>
#include <caml/fail.h>
#include <caml/callback.h>
#include <caml/custom.h>
#include <caml/bigarray.h>
#include <caml/threads.h>

#include <string.h>
#include <assert.h>
#include <stdio.h>

#include <libswscale/swscale.h>

#include "avutil_stubs.h"

/***** Filters *****/

/*
#define Filter_val(v) (*(SwsFilter**)Data_custom_val(v))

static void finalize_filter(value v)
{
  SwsFilter *f = Filter_val(v);
  sws_freeFilter(f);
}

static struct custom_operations filter_ops =
  {
    "ocaml_swscale_filter",
    finalize_filter,
    custom_compare_default,
    custom_hash_default,
    custom_serialize_default,
    custom_deserialize_default
  };
*/

/***** Contexts *****/

static const int FLAGS[] = { SWS_FAST_BILINEAR, SWS_BILINEAR, SWS_BICUBIC };

static int Flag_val(value v)
{
  return FLAGS[Int_val(v)];
}


#define Context_val(v) (*(struct SwsContext**)Data_custom_val(v))

static void finalize_context(v)
{
  struct SwsContext *c = Context_val(v);
  sws_freeContext(c);
}

static struct custom_operations context_ops =
  {
    "ocaml_swscale_context",
    finalize_context,
    custom_compare_default,
    custom_hash_default,
    custom_serialize_default,
    custom_deserialize_default
  };

CAMLprim value ocaml_swscale_get_context(value flags_, value src_w_, value src_h_, value src_format_, value dst_w_, value dst_h_, value dst_format_)
{
  CAMLparam1(flags_);
  CAMLlocal1(ans);
  int src_w = Int_val(src_w_);
  int src_h = Int_val(src_h_);
  enum PixelFormat src_format = PixelFormat_val(src_format_);
  int dst_w = Int_val(dst_w_);
  int dst_h = Int_val(dst_h_);
  enum PixelFormat dst_format = PixelFormat_val(dst_format_);
  int flags = 0;
  int i;
  struct SwsContext *c;

  for (i = 0; i < Wosize_val(flags_); i++)
    flags |= Flag_val(Field(flags_,i));

  caml_release_runtime_system();
  c = sws_getContext(src_w, src_h, src_format, dst_w, dst_h, dst_format, flags, NULL, NULL, NULL);
  caml_acquire_runtime_system();

  assert(c);
  ans = caml_alloc_custom(&context_ops, sizeof(struct SwsContext*), 0, 1);
  Context_val(ans) = c;
  CAMLreturn(ans);
}

CAMLprim value ocaml_swscale_get_context_byte(value *argv, int argn)
{
  /* TODO */
  assert(0);
}

CAMLprim value ocaml_swscale_scale(value context_, value src_, value off_, value h_, value dst_)
{
  CAMLparam3(context_, src_, dst_);
  struct SwsContext *context = Context_val(context_);
  int src_planes = Wosize_val(src_);
  int dst_planes = Wosize_val(dst_);
  int off = Int_val(off_);
  int h = Int_val(h_);
  int i;

  const uint8_t *src_slice[src_planes];
  int src_stride[src_planes];
  uint8_t *dst_slice[dst_planes];
  int dst_stride[dst_planes];

  for (i = 0; i < src_planes; i++)
    {
      src_slice[i] = Caml_ba_data_val(Field(src_, 0));
      src_stride[i] = Int_val(Field(src_, 1));
    }
  for (i = 0; i < dst_planes; i++)
    {
      dst_slice[i] = Caml_ba_data_val(Field(dst_, 0));
      dst_stride[i] = Int_val(Field(dst_, 1));
    }

  caml_release_runtime_system();
  sws_scale(context, src_slice, src_stride, off, h, dst_slice, dst_stride);
  caml_acquire_runtime_system();

  CAMLreturn(Val_unit);
}
