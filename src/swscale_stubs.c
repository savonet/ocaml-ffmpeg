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

#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>

#include "avutil_stubs.h"

#define ALIGNMENT_BYTES 16

CAMLprim value ocaml_swscale_version(value unit)
{
  CAMLparam0();
  CAMLreturn(Val_int(swscale_version()));
}

CAMLprim value ocaml_swscale_configuration(value unit)
{
  CAMLparam0();
  CAMLreturn(caml_copy_string(swscale_configuration()));
}

CAMLprim value ocaml_swscale_license(value unit)
{
  CAMLparam0();
  CAMLreturn(caml_copy_string(swscale_license()));
}

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

static const int FLAGS[] = { SWS_FAST_BILINEAR, SWS_BILINEAR, SWS_BICUBIC, SWS_PRINT_INFO };

static int Flag_val(value v)
{
  return FLAGS[Int_val(v)];
}

#define Context_val(v) (*(struct SwsContext**)Data_custom_val(v))

CAMLprim value ocaml_swscale_finalize_context(value v)
{
  CAMLparam1(v);
  caml_register_generational_global_root(&v);
  struct SwsContext *c = Context_val(v);
  caml_release_runtime_system();
  sws_freeContext(c);
  caml_acquire_runtime_system();
  caml_remove_generational_global_root(&v);
  CAMLreturn(Val_unit);
}

static struct custom_operations context_ops =
  {
    "ocaml_swscale_context",
    custom_finalize_default,
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
  enum AVPixelFormat src_format = PixelFormat_val(src_format_);
  int dst_w = Int_val(dst_w_);
  int dst_h = Int_val(dst_h_);
  enum AVPixelFormat dst_format = PixelFormat_val(dst_format_);
  int flags = 0;
  int i;
  struct SwsContext *c;

  for (i = 0; i < Wosize_val(flags_); i++)
    flags |= Flag_val(Field(flags_,i));

  caml_release_runtime_system();
  c = sws_getContext(src_w, src_h, src_format, dst_w, dst_h, dst_format, flags, NULL, NULL, NULL);
  caml_acquire_runtime_system();

  if (!c) Fail("Failed to get sws context!");

  ans = caml_alloc_custom(&context_ops, sizeof(struct SwsContext*), 0, 1);
  Context_val(ans) = c;

  Finalize("ocaml_swscale_finalize_context", ans);

  CAMLreturn(ans);
}

CAMLprim value ocaml_swscale_get_context_byte(value *argv, int argn)
{
  return ocaml_swscale_get_context(argv[0], argv[1], argv[2], argv[3], argv[4], argv[5], argv[6]);
}

CAMLprim value ocaml_swscale_scale(value context_, value src_, value off_, value h_, value dst_, value dst_off)
{
  CAMLparam4(context_, src_, dst_, dst_off);
  CAMLlocal1(v);
  struct SwsContext *context = Context_val(context_);
  int src_planes = Wosize_val(src_);
  int dst_planes = Wosize_val(dst_);
  int off = Int_val(off_);
  int h = Int_val(h_);
  int i;

  const uint8_t *src_slice[4];
  int src_stride[4];
  uint8_t *dst_slice[4];
  int dst_stride[4];

  memset(src_slice, 0, 4*sizeof(uint8_t*));
  memset(dst_slice, 0, 4*sizeof(uint8_t*));

  for (i = 0; i < src_planes; i++)
    {
      v = Field(src_, i);
      src_slice[i] = Caml_ba_data_val(Field(v, 0));
      src_stride[i] = Int_val(Field(v, 1));
    }
  for (i = 0; i < dst_planes; i++)
    {
      v = Field(dst_, i);
      dst_slice[i] = Caml_ba_data_val(Field(v, 0)) + Int_val(dst_off);
      dst_stride[i] = Int_val(Field(v, 1));
    }

  caml_release_runtime_system();
  sws_scale(context, src_slice, src_stride, off, h, dst_slice, dst_stride);
  caml_acquire_runtime_system();

  CAMLreturn(Val_unit);
}

CAMLprim value ocaml_swscale_scale_byte(value *argv, int argn)
{
  return ocaml_swscale_scale(argv[0], argv[1], argv[2], argv[3], argv[4], argv[5]);
}


/***** Contexts *****/

typedef enum _vector_kind {Ba, Frm, Str} vector_kind;

struct video_t {
  int width;
  int height;
  enum AVPixelFormat pixel_format;
  int nb_planes;
  uint8_t *slice_tab[4];
  int stride_tab[4];
  int sizes_tab[4];
  uint8_t **slice;
  int *stride;
  int owns_data;
};

typedef struct sws_t sws_t;

struct sws_t {
  struct SwsContext *context;
  int srcSliceY;
  int srcSliceH;
  struct video_t in;
  struct video_t out;
  value out_vector;
  int release_out_vector;

  int (*get_in_pixels)(sws_t *, value *);
  int (*alloc_out)(sws_t *);
  int (*copy_out)(sws_t *);
};

#define Sws_val(v) (*(sws_t**)Data_custom_val(v))

static int get_in_pixels_frame(sws_t *sws, value *in_vector)
{
  AVFrame *frame = Frame_val(*in_vector);

  sws->in.slice = frame->data;
  sws->in.stride = frame->linesize;

  return 0;
}

static int get_in_pixels_string(sws_t *sws, value *in_vector)
{
  CAMLparam0();
  CAMLlocal2(v, str);
  int i, nb_planes = Wosize_val(*in_vector) > 4 ? 4 : Wosize_val(*in_vector);

  for (i = 0; i < nb_planes; i++) {
    v = Field(*in_vector, i);
    str = Field(v, 0);
    sws->in.stride[i] = Int_val(Field(v, 1));
    size_t str_len = caml_string_length(str);

    if(sws->in.sizes_tab[i] < str_len) {
      sws->in.slice[i] = (uint8_t*)realloc(sws->in.slice[i], str_len);
      sws->in.sizes_tab[i] = str_len;
    }

    memcpy(sws->in.slice[i], (uint8_t*)String_val(str), str_len);
  }
  
  CAMLreturnT(int, nb_planes);
}

static int get_in_pixels_ba(sws_t *sws, value *in_vector)
{
  CAMLparam0();
  CAMLlocal1(v);
  int i, nb_planes = Wosize_val(*in_vector);

  for (i = 0; i < nb_planes && i < 4; i++) {
    v = Field(*in_vector, i);
    sws->in.slice[i] = Caml_ba_data_val(Field(v, 0));
    sws->in.stride[i] = Int_val(Field(v, 1));
  }
  
  CAMLreturnT(int, nb_planes);
}


static int alloc_out_frame(sws_t *sws)
{
  CAMLparam0();
  CAMLlocal1(v);
  int ret;
#ifndef HAS_FRAME
  caml_failwith("Not implemented.");
#else
  do {
    caml_release_runtime_system();
    AVFrame * frame = av_frame_alloc();
    caml_acquire_runtime_system();

    if (!frame) caml_raise_out_of_memory();

    frame->width  = sws->out.width;
    frame->height = sws->out.height;
    frame->format = sws->out.pixel_format;

    // allocate the buffers for the frame data
    caml_release_runtime_system();

    ret = av_frame_get_buffer(frame, 32);

    if (ret < 0) {
      av_frame_free(&frame);
      caml_acquire_runtime_system();
      ocaml_avutil_raise_error(ret);
    }

    caml_acquire_runtime_system();

    sws->out.slice = frame->data;
    sws->out.stride = frame->linesize;

    v = value_of_frame(frame);
    caml_modify_generational_global_root(&sws->out_vector, v);
  } while(0);
#endif
  CAMLreturnT(int, ret);
}

static int alloc_out_string(sws_t *sws)
{
  CAMLparam0();
  CAMLlocal1(v);
  int i;

  caml_modify_generational_global_root(&sws->out_vector, caml_alloc_tuple(sws->out.nb_planes));

  for(i = 0; i < sws->out.nb_planes; i++) {
    int len = sws->out.stride[i] * sws->out.height;

    if(sws->out.sizes_tab[i] < len) {
      sws->out.slice[i] = (uint8_t*)realloc(sws->out.slice[i], len + 16);
      sws->out.sizes_tab[i] = len;
    }

    v = caml_alloc_tuple(2);
    Store_field(v, 0, caml_alloc_string(len));
    Store_field(v, 1, Val_int(sws->out.stride[i]));

    Store_field(sws->out_vector, i, v);
  }

  CAMLreturnT(int, 0);
}

static int copy_out_string(sws_t *sws)
{
  CAMLparam0();
  CAMLlocal1(str);
  int i;

  for(i = 0; i < sws->out.nb_planes; i++) {
    str = Field(Field(sws->out_vector, i), 0);

    memcpy((uint8_t*)String_val(str), sws->out.slice[i], sws->out.sizes_tab[i]);
  }

  CAMLreturnT(int, 0);
}

static int alloc_out_ba(sws_t *sws)
{
  CAMLparam0();
  CAMLlocal1(v);
  int i;

  caml_modify_generational_global_root(&sws->out_vector, caml_alloc_tuple(sws->out.nb_planes));

  for(i = 0; i < sws->out.nb_planes; i++) {
    // Some filters and swscale can read up to 16 bytes beyond the planes, 16 extra bytes must be allocated.
    intnat out_size = sws->out.stride[i] * sws->out.height + 16;

    v = caml_alloc_tuple(2);
    Store_field(v, 0, caml_ba_alloc(CAML_BA_C_LAYOUT | CAML_BA_UINT8, 1, NULL, &out_size));
    Store_field(v, 1, Val_int(sws->out.stride[i]));

    sws->out.slice[i] = Caml_ba_data_val(Field(v, 0));

    Store_field(sws->out_vector, i, v);
  }

  CAMLreturnT(int, 0);
}

CAMLprim value ocaml_swscale_convert(value _sws, value _in_vector)
{
  CAMLparam2(_sws, _in_vector);
  sws_t *sws = Sws_val(_sws);

  // acquisition of the input pixels
  int ret = sws->get_in_pixels(sws, &_in_vector);
  if(ret < 0) Fail( "Failed to get input pixels");

  // Allocate out data if needed
  if (sws->release_out_vector) {
    ret = sws->alloc_out(sws);
    if(ret < 0) ocaml_avutil_raise_error(ret);
  }

  // Scale and convert input data to output data
  caml_release_runtime_system();
  ret = sws_scale(sws->context,
                  (const uint8_t * const*)sws->in.slice, sws->in.stride,
                  sws->srcSliceY, sws->srcSliceH,
                  sws->out.slice, sws->out.stride);
  caml_acquire_runtime_system();

  if(ret < 0) ocaml_avutil_raise_error(ret);

  if(sws->copy_out) {
    ret = sws->copy_out(sws);
    if(ret < 0) ocaml_avutil_raise_error(ret);
  }
  
  CAMLreturn(sws->out_vector);
}

void swscale_free(sws_t *sws)
{
  int i;

  if(sws->context) sws_freeContext(sws->context);

  if(sws->in.owns_data) {
    for(i = 0; sws->in.slice[i]; i++) free(sws->in.slice[i]);
  }

  if(sws->out.owns_data) {
    for(i = 0; sws->out.slice[i]; i++) free(sws->out.slice[i]);
  }

  if(sws->out_vector) caml_remove_generational_global_root(&sws->out_vector);

  free(sws);
}

CAMLprim value ocaml_swscale_finalize_swscale(value v)
{
  CAMLparam1(v);
  caml_register_generational_global_root(&v);
  caml_release_runtime_system();
  swscale_free(Sws_val(v));
  caml_acquire_runtime_system();
  caml_remove_generational_global_root(&v);
  CAMLreturn(Val_unit);
}

static struct custom_operations sws_ops =
  {
    "ocaml_swscale_context",
    custom_finalize_default,
    custom_compare_default,
    custom_hash_default,
    custom_serialize_default,
    custom_deserialize_default
  };

CAMLprim value ocaml_swscale_create(value flags_, value in_vector_kind_, value in_width_, value in_height_, value in_pixel_format_, value out_vector_kind_, value out_width_, value out_height_, value out_pixel_format_)
{
  CAMLparam1(flags_);
  CAMLlocal1(ans);
  vector_kind in_vector_kind = Int_val(in_vector_kind_);
  vector_kind out_vector_kind = Int_val(out_vector_kind_);
  int flags = 0, i;

  sws_t * sws = (sws_t*)calloc(1, sizeof(sws_t));

  if( ! sws) Fail( "Failed to create Swscale context");

  sws->in.slice = sws->in.slice_tab;
  sws->in.stride = sws->in.stride_tab;

  sws->in.width = Int_val(in_width_);
  sws->in.height = Int_val(in_height_);
  sws->in.pixel_format = PixelFormat_val(in_pixel_format_);

  sws->srcSliceH = sws->in.height;

  sws->out.slice = sws->out.slice_tab;
  sws->out.stride = sws->out.stride_tab;

  sws->out.width = Int_val(out_width_);
  sws->out.height = Int_val(out_height_);
  sws->out.pixel_format = PixelFormat_val(out_pixel_format_);

  for (i = 0; i < Wosize_val(flags_); i++)
    flags |= Flag_val(Field(flags_, i));
  
  caml_release_runtime_system();
  sws->context = sws_getContext(sws->in.width, sws->in.height, sws->in.pixel_format,
                                sws->out.width, sws->out.height, sws->out.pixel_format,
                                flags, NULL, NULL, NULL);
  caml_acquire_runtime_system();

  if( ! sws->context) {
    free(sws);
    Fail("Failed to create Swscale context");
  }

  sws->release_out_vector = 1;
  
  if(in_vector_kind == Frm) {
    sws->get_in_pixels = get_in_pixels_frame;
  }
  else if(in_vector_kind == Str) {
    sws->get_in_pixels = get_in_pixels_string;
    sws->in.owns_data = 1;
  }
  else {
    sws->get_in_pixels = get_in_pixels_ba;
  }

  sws->out_vector = Val_unit;
  caml_register_generational_global_root(&sws->out_vector);

  if(out_vector_kind == Frm) {
    sws->alloc_out = alloc_out_frame;
  }
  else if(out_vector_kind == Str) {
    sws->alloc_out = alloc_out_string;
    sws->copy_out = copy_out_string;
    sws->out.owns_data = 1;
  }
  else {
    sws->alloc_out = alloc_out_ba;
  }

  caml_release_runtime_system();
  int ret = av_image_fill_linesizes(sws->out.stride, sws->out.pixel_format, sws->out.width);

  if(ret < 0) {
    swscale_free(sws);
    caml_acquire_runtime_system();
    Fail( "Failed to create Swscale context");
  }

  for(sws->out.nb_planes = 0; sws->out.stride[sws->out.nb_planes]; sws->out.nb_planes++);

  ret = sws->alloc_out(sws);
  if(ret < 0) {
    swscale_free(sws);
    caml_acquire_runtime_system();
    Fail( "Failed to create Swscale context");
  }

  caml_acquire_runtime_system();

  ans = caml_alloc_custom(&sws_ops, sizeof(sws_t*), 0, 1);
  Sws_val(ans) = sws;

  Finalize("ocaml_swscale_finalize_swscale", ans);

  CAMLreturn(ans);
}

CAMLprim value ocaml_swscale_create_byte(value *argv, int argn)
{
  return ocaml_swscale_create(argv[0], argv[1], argv[2], argv[3], argv[4], argv[5], argv[6], argv[7], argv[8]);
}

CAMLprim value ocaml_swscale_reuse_output(value _sws, value _reuse_output)
{
  CAMLparam2(_sws, _reuse_output);
  Sws_val(_sws)->release_out_vector = ! Bool_val(_reuse_output);
  CAMLreturn(Val_unit);
}
