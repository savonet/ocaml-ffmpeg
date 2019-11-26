#include <caml/memory.h>
#include <caml/custom.h>
#include <caml/fail.h>
#include <caml/callback.h>
#include <caml/threads.h>

#include <libavfilter/avfilter.h>
#include <libavfilter/buffersrc.h>
#include <libavfilter/buffersink.h>
#include "avutil_stubs.h"

CAMLprim value ocaml_avfilter_register_all(value unit) {
  CAMLparam0();
#if LIBAVFILTER_VERSION_INT < AV_VERSION_INT(7, 14, 100)
  avfilter_register_all();
#endif
  CAMLreturn(Val_unit);
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

CAMLprim value ocaml_avfilter_create_filter(value _instance_name, value _args, value _name, value _graph) {
  CAMLparam4(_instance_name, _args, _name, _graph);
  CAMLlocal1(ret);

  char *name = NULL;
  char *args = NULL;
  AVFilterGraph *graph = Filter_graph_val(_graph);
  const AVFilter *filter = avfilter_get_by_name(String_val(_name));
  AVFilterContext *context;
  int err;

  if (!filter) caml_raise_not_found();

  if (_instance_name != Val_none) {
    name = strndup(String_val(Some_val(_instance_name)), caml_string_length(Some_val(_instance_name)));
    if (!name) caml_raise_out_of_memory();
  }

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
