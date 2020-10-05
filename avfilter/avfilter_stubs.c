#include <caml/alloc.h>
#include <caml/callback.h>
#include <caml/custom.h>
#include <caml/fail.h>
#include <caml/memory.h>
#include <caml/threads.h>

#include "avutil_stubs.h"
#include "polymorphic_variant_values_stubs.h"
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>

CAMLprim value ocaml_avfilter_register_all(value unit) {
  CAMLparam0();
#if LIBAVFILTER_VERSION_INT < AV_VERSION_INT(7, 14, 100)
  avfilter_register_all();
#endif
  CAMLreturn(Val_unit);
}

CAMLprim value ocaml_avfilter_get_all_filters(value unit) {
  CAMLparam0();
  CAMLlocal4(pad, pads, cur, ret);
  int c = 0;
  int i, pad_count;
  const AVFilter *f = NULL;
  int pad_type;

#if LIBAVFILTER_VERSION_INT >= AV_VERSION_INT(7, 14, 100)
  void *opaque = 0;
#endif

#if LIBAVFILTER_VERSION_INT < AV_VERSION_INT(7, 14, 100)
  while ((f = avfilter_next(f)))
    c++;
#else
  while ((f = av_filter_iterate(&opaque)))
    c++;
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
    cur = caml_alloc_tuple(6);
    Store_field(cur, 0, caml_copy_string(f->name));
    Store_field(cur, 1, caml_copy_string(f->description));

    pad_count = avfilter_pad_count(f->inputs);
    pads = caml_alloc_tuple(pad_count);
    for (i = 0; i < pad_count; i++) {
      pad = caml_alloc_tuple(6);
      Store_field(pad, 0,
                  caml_copy_string(avfilter_pad_get_name(f->inputs, i)));
      Store_field(pad, 1, caml_copy_string(f->name));

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

      Store_field(pad, 2, pad_type);
      Store_field(pad, 3, Val_int(i));
      Store_field(pad, 4, Val_none);
      Store_field(pad, 5, Val_none);

      Store_field(pads, i, pad);
    }
    Store_field(cur, 2, pads);

    pad_count = avfilter_pad_count(f->outputs);
    pads = caml_alloc_tuple(pad_count);
    for (i = 0; i < pad_count; i++) {
      pad = caml_alloc_tuple(6);
      Store_field(pad, 0,
                  caml_copy_string(avfilter_pad_get_name(f->outputs, i)));
      Store_field(pad, 1, caml_copy_string(f->name));

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

      Store_field(pad, 2, pad_type);
      Store_field(pad, 3, Val_int(i));
      Store_field(pad, 4, Val_none);
      Store_field(pad, 5, Val_none);

      Store_field(pads, i, pad);
    }
    Store_field(cur, 3, pads);
    Store_field(cur, 4, (value)f->priv_class);
    Store_field(cur, 5, Val_int(f->flags));

    Store_field(ret, c, cur);
    c++;
  }

  CAMLreturn(ret);
}

#define Filter_graph_val(v) (*(AVFilterGraph **)Data_custom_val(v))

static void finalize_filter_graph(value v) {
  AVFilterGraph *graph = Filter_graph_val(v);
  avfilter_graph_free(&graph);
}

static struct custom_operations filter_graph_ops = {
    "ocaml_avfilter_filter_graph", finalize_filter_graph,
    custom_compare_default,        custom_hash_default,
    custom_serialize_default,      custom_deserialize_default};

CAMLprim value ocaml_avfilter_init(value unit) {
  CAMLparam0();
  CAMLlocal1(ret);
  AVFilterGraph *graph = avfilter_graph_alloc();

  if (!graph)
    caml_raise_out_of_memory();

  ret = caml_alloc_custom(&filter_graph_ops, sizeof(AVFilterGraph *), 1, 0);

  Filter_graph_val(ret) = graph;

  CAMLreturn(ret);
}

CAMLprim value ocaml_avfilter_create_filter(value _args, value _instance_name,
                                            value _name, value _graph) {
  CAMLparam4(_instance_name, _args, _name, _graph);
  CAMLlocal1(ret);

  char *name = NULL;
  char *args = NULL;
  AVFilterGraph *graph = Filter_graph_val(_graph);
  const AVFilter *filter = avfilter_get_by_name(String_val(_name));
  AVFilterContext *context;
  int err;

  if (!filter)
    caml_raise_not_found();

  name =
      strndup(String_val(_instance_name), caml_string_length(_instance_name));
  if (!name)
    caml_raise_out_of_memory();

  if (_args != Val_none) {
    args = strndup(String_val(Some_val(_args)),
                   caml_string_length(Some_val(_args)));

    if (!args) {
      if (name)
        free(name);
      caml_raise_out_of_memory();
    }
  }

  caml_release_runtime_system();
  err = avfilter_graph_create_filter(&context, filter, name, args, NULL, graph);
  caml_acquire_runtime_system();

  if (name)
    free(name);
  if (args)
    free(args);

  if (err < 0)
    ocaml_avutil_raise_error(err);

  CAMLreturn((value)context);
}

static void append_avfilter_in_out(AVFilterInOut **filter, char *name,
                                   AVFilterContext *filter_ctx, int pad_idx) {
  AVFilterInOut *cur;

  if (*filter) {
    cur = *filter;
    while (cur)
      cur = cur->next;
    cur->next = avfilter_inout_alloc();
    cur = cur->next;
  } else {
    *filter = avfilter_inout_alloc();
    cur = *filter;
  }

  if (!cur) {
    avfilter_inout_free(filter);
    caml_raise_out_of_memory();
  }

  cur->name = name;
  cur->filter_ctx = filter_ctx;
  cur->pad_idx = pad_idx;
  cur->next = NULL;
};

CAMLprim value ocaml_avfilter_parse(value _inputs, value _outputs,
                                    value _filters, value _graph) {
  CAMLparam4(_inputs, _outputs, _filters, _graph);
  CAMLlocal1(_pad);

  int c, err, idx;
  AVFilterInOut *inputs = NULL;
  AVFilterInOut *outputs = NULL;
  AVFilterGraph *graph = Filter_graph_val(_graph);
  AVFilterContext *filter_ctx;
  char *filters, *name;

  for (c = 0; c < Wosize_val(_inputs); c++) {
    _pad = Field(_inputs, c);
    name = av_strdup(String_val(Field(_pad, 0)));
    filter_ctx = (AVFilterContext *)Field(_pad, 1);
    idx = Int_val(Field(_pad, 2));

    append_avfilter_in_out(&inputs, name, filter_ctx, idx);
  }

  for (c = 0; c < Wosize_val(_outputs); c++) {
    _pad = Field(_outputs, c);
    name = av_strdup(String_val(Field(_pad, 0)));
    filter_ctx = (AVFilterContext *)Field(_pad, 1);
    idx = Int_val(Field(_pad, 2));

    append_avfilter_in_out(&outputs, name, filter_ctx, idx);
  }

  filters = strndup(String_val(_filters), caml_string_length(_filters));

  if (!filters) {
    if (inputs)
      avfilter_inout_free(&inputs);
    if (outputs)
      avfilter_inout_free(&outputs);
    caml_raise_out_of_memory();
  }

  caml_release_runtime_system();
  err = avfilter_graph_parse_ptr(graph, filters, &inputs, &outputs, NULL);
  caml_acquire_runtime_system();

  free(filters);

  if (inputs)
    avfilter_inout_free(&inputs);

  if (outputs)
    avfilter_inout_free(&outputs);

  if (err < 0)
    ocaml_avutil_raise_error(err);

  CAMLreturn(Val_unit);
}

CAMLprim value ocaml_avfilter_link(value _src, value _srcpad, value _dst,
                                   value _dstpad) {
  CAMLparam0();

  caml_release_runtime_system();
  int err = avfilter_link((AVFilterContext *)_src, Int_val(_srcpad),
                          (AVFilterContext *)_dst, Int_val(_dstpad));
  caml_acquire_runtime_system();

  if (err < 0)
    ocaml_avutil_raise_error(err);

  CAMLreturn(Val_unit);
}

CAMLprim value ocaml_avfilter_buffersink_get_time_base(value _src) {
  CAMLparam0();
  CAMLlocal1(ret);

  caml_release_runtime_system();
  AVRational time_base = av_buffersink_get_time_base((AVFilterContext *)_src);
  caml_acquire_runtime_system();

  value_of_rational(&time_base, &ret);

  CAMLreturn(ret);
}

CAMLprim value ocaml_avfilter_buffersink_get_frame_rate(value _src) {
  CAMLparam0();
  CAMLlocal1(ret);

  caml_release_runtime_system();
  AVRational frame_rate = av_buffersink_get_frame_rate((AVFilterContext *)_src);
  caml_acquire_runtime_system();

  value_of_rational(&frame_rate, &ret);

  CAMLreturn(ret);
}

CAMLprim value ocaml_avfilter_buffersink_get_sample_format(value _src) {
  CAMLparam0();

  caml_release_runtime_system();
  int sample_format = av_buffersink_get_format((AVFilterContext *)_src);
  caml_acquire_runtime_system();

  CAMLreturn(Val_SampleFormat((enum AVSampleFormat)sample_format));
}

CAMLprim value ocaml_avfilter_buffersink_get_w(value _src) {
  CAMLparam0();

  caml_release_runtime_system();
  int w = av_buffersink_get_w((AVFilterContext *)_src);
  caml_acquire_runtime_system();

  CAMLreturn(Val_int(w));
}

CAMLprim value ocaml_avfilter_buffersink_get_h(value _src) {
  CAMLparam0();

  caml_release_runtime_system();
  int h = av_buffersink_get_h((AVFilterContext *)_src);
  caml_acquire_runtime_system();

  CAMLreturn(Val_int(h));
}

CAMLprim value ocaml_avfilter_buffersink_get_pixel_format(value _src) {
  CAMLparam0();

  caml_release_runtime_system();
  int pixel_format = av_buffersink_get_format((AVFilterContext *)_src);
  caml_acquire_runtime_system();

  CAMLreturn(Val_PixelFormat((enum AVPixelFormat)pixel_format));
}

CAMLprim value ocaml_avfilter_buffersink_get_sample_aspect_ratio(value _src) {
  CAMLparam0();
  CAMLlocal1(ret);

  caml_release_runtime_system();
  AVRational sample_aspect_ratio =
      av_buffersink_get_sample_aspect_ratio((AVFilterContext *)_src);
  caml_acquire_runtime_system();

  value_of_rational(&sample_aspect_ratio, &ret);

  CAMLreturn(ret);
}

CAMLprim value ocaml_avfilter_buffersink_get_channels(value _src) {
  CAMLparam0();

  caml_release_runtime_system();
  int channels = av_buffersink_get_channels((AVFilterContext *)_src);
  caml_acquire_runtime_system();

  CAMLreturn(Val_int(channels));
}

CAMLprim value ocaml_avfilter_buffersink_get_channel_layout(value _src) {
  CAMLparam0();

  caml_release_runtime_system();
  uint64_t layout = av_buffersink_get_channel_layout((AVFilterContext *)_src);
  caml_acquire_runtime_system();

  CAMLreturn(Val_ChannelLayout(layout));
}

CAMLprim value ocaml_avfilter_buffersink_get_sample_rate(value _src) {
  CAMLparam0();

  caml_release_runtime_system();
  int sample_rate = av_buffersink_get_sample_rate((AVFilterContext *)_src);
  caml_acquire_runtime_system();

  CAMLreturn(Val_int(sample_rate));
}

CAMLprim value ocaml_avfilter_buffersink_set_frame_size(value _src,
                                                        value _size) {
  CAMLparam0();

  caml_release_runtime_system();
  av_buffersink_set_frame_size((AVFilterContext *)_src, Int_val(_size));
  caml_acquire_runtime_system();

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

CAMLprim value ocaml_avfilter_write_frame(value _config, value _filter,
                                          value _frame) {
  CAMLparam2(_config, _frame);

  caml_release_runtime_system();
  int err =
      av_buffersrc_write_frame((AVFilterContext *)_filter, Frame_val(_frame));
  caml_acquire_runtime_system();

  if (err < 0)
    ocaml_avutil_raise_error(err);

  CAMLreturn(Val_unit);
}

CAMLprim value ocaml_avfilter_get_frame(value _config, value _filter) {
  CAMLparam1(_config);
  CAMLlocal1(frame_value);

  caml_release_runtime_system();
  AVFrame *frame = av_frame_alloc();
  caml_acquire_runtime_system();

  if (!frame)
    caml_raise_out_of_memory();

  frame_value = value_of_frame(frame);

  caml_release_runtime_system();
  int err = av_buffersink_get_frame((AVFilterContext *)_filter, frame);
  caml_acquire_runtime_system();

  if (err < 0)
    ocaml_avutil_raise_error(err);

  CAMLreturn(frame_value);
}

CAMLprim value ocaml_avfilter_int_of_flag(value _flag) {
  CAMLparam1(_flag);

  switch (_flag) {
  case PVV_Dynamic_inputs:
    CAMLreturn(Val_int(AVFILTER_FLAG_DYNAMIC_INPUTS));
  case PVV_Dynamic_outputs:
    CAMLreturn(Val_int(AVFILTER_FLAG_DYNAMIC_OUTPUTS));
  case PVV_Slice_threads:
    CAMLreturn(Val_int(AVFILTER_FLAG_SLICE_THREADS));
  case PVV_Support_timeline_generic:
    CAMLreturn(Val_int(AVFILTER_FLAG_SUPPORT_TIMELINE_GENERIC));
  case PVV_Support_timeline_internal:
    CAMLreturn(Val_int(AVFILTER_FLAG_SUPPORT_TIMELINE_INTERNAL));
  default:
    caml_failwith("Invalid flag type!");
  }
}
