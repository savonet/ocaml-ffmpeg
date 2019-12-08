#include <caml/alloc.h>
#include <caml/memory.h>
#include <caml/custom.h>
#include <caml/fail.h>
#include <caml/callback.h>
#include <caml/threads.h>

#include <libavfilter/avfilter.h>
#include <libavfilter/buffersrc.h>
#include <libavfilter/buffersink.h>
#include "avutil_stubs.h"
#include "polymorphic_variant_values_stubs.h"

CAMLprim value ocaml_avfilter_register_all(value unit) {
  CAMLparam0();
#if LIBAVFILTER_VERSION_INT < AV_VERSION_INT(7, 14, 100)
  avfilter_register_all();
#endif
  CAMLreturn(Val_unit);
}

CAMLprim value ocaml_avfilter_get_all_filters(value unit) {
  CAMLparam0();
  CAMLlocal4(pad,pads,cur,ret);
  int c = 0;
  int i, pad_count;
  const AVFilter *f = NULL;
  int pad_type;

#if LIBAVFILTER_VERSION_INT >= AV_VERSION_INT(7, 14, 100)
  void *opaque = 0;
#endif
 
#if LIBAVFILTER_VERSION_INT < AV_VERSION_INT(7, 14, 100)
  while ((f = avfilter_next(f))) c++;
#else
  while ((f = av_filter_iterate(&opaque))) c++;
#endif

  ret = caml_alloc_tuple(c);

  c = 0;
  f = NULL;

#if LIBAVFILTER_VERSION_INT < AV_VERSION_INT(7, 14, 100)
  while ((f = avfilter_next(f))) {
#else
  opaque = 0;
  while ((f = av_filter_iterate(&opaque))) {
#endif
    cur = caml_alloc_tuple(4);
    Store_field(cur, 0, caml_copy_string(f->name));
    Store_field(cur, 1, caml_copy_string(f->description));

    pad_count = avfilter_pad_count(f->inputs);
    pads = caml_alloc_tuple(pad_count);
    for (i = 0; i < pad_count; i++) {
      pad = caml_alloc_tuple(5);
      Store_field(pad, 0, caml_copy_string(avfilter_pad_get_name(f->inputs, i)));

      switch (avfilter_pad_get_type(f->inputs, i)) {
        case AVMEDIA_TYPE_VIDEO:
          pad_type = PVV_Video;
          break;
        case AVMEDIA_TYPE_AUDIO:
          pad_type = PVV_Audio;
          break;
        case AVMEDIA_TYPE_DATA:
          pad_type = PVV_Data;
          break;
        case AVMEDIA_TYPE_SUBTITLE:
          pad_type = PVV_Subtitle;
          break;
        case AVMEDIA_TYPE_ATTACHMENT:
          pad_type = PVV_Attachment;
          break;
        default:
          pad_type = PVV_Unknown;
      }

      Store_field(pad, 1, pad_type);
      Store_field(pad, 2, Val_int(i));
      Store_field(pad, 3, Val_none);
      Store_field(pad, 4, Val_none);

      Store_field(pads, i, pad);
    }
    Store_field(cur, 2, pads);

    pad_count = avfilter_pad_count(f->outputs);
    pads = caml_alloc_tuple(pad_count);
    for (i = 0; i < pad_count; i++) {
      pad = caml_alloc_tuple(5);
      Store_field(pad, 0, caml_copy_string(avfilter_pad_get_name(f->outputs, i)));

      switch (avfilter_pad_get_type(f->outputs, i)) {
        case AVMEDIA_TYPE_VIDEO:
          pad_type = PVV_Video;
          break;
        case AVMEDIA_TYPE_AUDIO:
          pad_type = PVV_Audio;
          break;
        case AVMEDIA_TYPE_DATA:
          pad_type = PVV_Data;
          break;
        case AVMEDIA_TYPE_SUBTITLE:
          pad_type = PVV_Subtitle;
          break;
        case AVMEDIA_TYPE_ATTACHMENT:
          pad_type = PVV_Attachment;
          break;
        default:
          pad_type = PVV_Unknown;
      }

      Store_field(pad, 1, pad_type);
      Store_field(pad, 2, Val_int(i));
      Store_field(pad, 3, Val_none);
      Store_field(pad, 4, Val_none);

      Store_field(pads, i, pad);
    }
    Store_field(cur, 3, pads);

    Store_field(ret, c, cur);
    c++;
  }
  
  CAMLreturn(ret);
}

#define Filter_graph_val(v) (*(AVFilterGraph**)Data_custom_val(v))

CAMLprim value ocaml_avfilter_finalize_filter_graph(value v)
{
  CAMLparam1(v);
  caml_register_generational_global_root(&v); 
  AVFilterGraph *graph = Filter_graph_val(v);
  caml_release_runtime_system();
  avfilter_graph_free(&graph);
  caml_acquire_runtime_system();
  caml_remove_generational_global_root(&v);
  CAMLreturn(Val_unit);
}

static struct custom_operations filter_graph_ops =
{
  "ocaml_avfilter_filter_graph",
  custom_finalize_default,
  custom_compare_default,
  custom_hash_default,
  custom_serialize_default,
  custom_deserialize_default
};

CAMLprim value ocaml_avfilter_init(value unit) {
  CAMLparam0();
  CAMLlocal1(ret);
  AVFilterGraph *graph = avfilter_graph_alloc();

  if (!graph) caml_raise_out_of_memory();

  ret = caml_alloc_custom(&filter_graph_ops, sizeof(AVFilterGraph*), 1, 0);

  Filter_graph_val(ret) = graph;

  Finalize("ocaml_avfilter_finalize_filter_graph", ret);

  CAMLreturn(ret);
}

CAMLprim value ocaml_avfilter_create_filter(value _args, value _instance_name, value _name, value _graph) {
  CAMLparam4(_instance_name, _args, _name, _graph);
  CAMLlocal1(ret);

  char *name = NULL;
  char *args = NULL;
  AVFilterGraph *graph = Filter_graph_val(_graph);
  const AVFilter *filter = avfilter_get_by_name(String_val(_name));
  AVFilterContext *context;
  int err;

  if (!filter) caml_raise_not_found();

  name = strndup(String_val(_instance_name), caml_string_length(_instance_name));
  if (!name) caml_raise_out_of_memory();

  if (_args != Val_none) {
    args = strndup(String_val(Some_val(_args)), caml_string_length(Some_val(_args)));

    if (!args) {
      if (name) free(name);
      caml_raise_out_of_memory();
    }
  }

  caml_release_runtime_system();
  err = avfilter_graph_create_filter(&context, filter, name, args, NULL, graph);
  caml_acquire_runtime_system();

  if (name) free(name);
  if (args) free(args);

  if (err < 0)
    ocaml_avutil_raise_error(err);

  CAMLreturn((value)context);
}

CAMLprim value ocaml_avfilter_link(value _src, value _srcpad, value _dst, value _dstpad) {
  CAMLparam0();

  caml_release_runtime_system();
  int err = avfilter_link((AVFilterContext *)_src, Int_val(_srcpad), (AVFilterContext *)_dst, Int_val(_dstpad));
  caml_acquire_runtime_system();

  if (err < 0)
    ocaml_avutil_raise_error(err);

  CAMLreturn(Val_unit);
}

CAMLprim value ocaml_avfilter_config(value _graph) {
  CAMLparam1(_graph);

  caml_release_runtime_system();
  int err = avfilter_graph_config(Filter_graph_val(_graph), NULL);
  caml_acquire_runtime_system();

  if (err < 0)
    ocaml_avutil_raise_error(err);

  CAMLreturn(Val_unit);
}

CAMLprim value ocaml_avfilter_write_frame(value _filter, value _frame) {
  CAMLparam1(_frame);

  caml_release_runtime_system();
  int err = av_buffersrc_write_frame((AVFilterContext *)_filter, Frame_val(_frame));
  caml_acquire_runtime_system();

  if (err < 0)
    ocaml_avutil_raise_error(err);

  CAMLreturn(Val_unit);
}

CAMLprim value ocaml_avfilter_get_frame(value _filter) {
  CAMLparam0();
  CAMLlocal1(frame_value);

  caml_release_runtime_system();
  AVFrame *frame = av_frame_alloc();
  caml_acquire_runtime_system();

  if (!frame) caml_raise_out_of_memory();

  frame_value = value_of_frame(frame);

  caml_release_runtime_system();
  int err = av_buffersink_get_frame((AVFilterContext *)_filter, frame);
  caml_acquire_runtime_system();

  if (err < 0)
    ocaml_avutil_raise_error(err);

  CAMLreturn(frame_value);
}
