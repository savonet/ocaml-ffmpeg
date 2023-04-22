#define CAML_NAME_SPACE 1

#include <caml/alloc.h>
#include <caml/bigarray.h>
#include <caml/callback.h>
#include <caml/custom.h>
#include <caml/fail.h>
#include <caml/memory.h>
#include <caml/mlvalues.h>
#include <caml/threads.h>

#ifndef Bytes_val
#define Bytes_val String_val
#endif

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>
#include <libswresample/swresample.h>

#include "avutil_stubs.h"
#include "swresample_options_stubs.h"
#include "swresample_stubs.h"

static inline double filter_nan(double s) {
  if (s != s)
    return 0;
  return s;
}

/***** Contexts *****/
struct audio_t {
  uint8_t **data;
  int nb_samples;
  int nb_channels;
  enum AVSampleFormat sample_fmt;
  int is_planar;
  int bytes_per_samples;
  int owns_data;
};

struct swr_t {
  SwrContext *context;
  struct audio_t in;
  struct audio_t out;
  int64_t out_channel_layout;
  int out_sample_rate;
  value out_vector;
  int out_vector_nb_samples;

  int (*get_in_samples)(swr_t *, value *, int);
  void (*convert)(swr_t *, int, int);
};

#define Swr_val(v) (*(swr_t **)Data_custom_val(v))

static void alloc_data(struct audio_t *audio, int nb_samples) {
  if (audio->data != NULL && audio->data[0] != NULL) {
    av_freep(&audio->data[0]);
    audio->nb_samples = 0;
  }

  audio->owns_data = 1;

  int ret = av_samples_alloc(audio->data, NULL, audio->nb_channels, nb_samples,
                             audio->sample_fmt, 0);

  if (ret < 0)
    ocaml_avutil_raise_error(ret);

  audio->nb_samples = nb_samples;
}

static int get_in_samples_frame(swr_t *swr, value *in_vector, int offset) {
  AVFrame *frame = Frame_val(*in_vector);
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(56, 0, 100)
  int nb_channels = av_frame_get_channels(frame);
#elif LIBAVFORMAT_VERSION_INT < AV_VERSION_INT(59, 19, 100)
  int nb_channels = frame->channels;
#else
  int nb_channels = frame->ch_layout.nb_channels;
#endif

  if (offset != 0)
    Fail("Cannot use offset with frame data!");

  if (nb_channels != swr->in.nb_channels)
    Fail("Swresample failed to convert %d channels : %d channels were expected",
         nb_channels, swr->in.nb_channels);

  if (frame->format != swr->in.sample_fmt)
    Fail("Swresample failed to convert %s sample format : %s sample format "
         "were expected",
         av_get_sample_fmt_name(frame->format),
         av_get_sample_fmt_name(swr->in.sample_fmt));

  swr->in.data = frame->extended_data;

  return frame->nb_samples;
}

static int get_in_samples_string(swr_t *swr, value *in_vector, int offset) {
  int str_len = caml_string_length(*in_vector);
  int bytes_per_sample = swr->in.bytes_per_samples * swr->in.nb_channels;
  int nb_samples = str_len / bytes_per_sample - offset;

  if (nb_samples < 0)
    Fail("Invalid offset!");

  if (nb_samples > swr->in.nb_samples)
    alloc_data(&swr->in, nb_samples);

  memcpy(swr->in.data[0],
         (uint8_t *)String_val(*in_vector) + offset * bytes_per_sample,
         str_len);

  return nb_samples;
}

static int get_in_samples_planar_string(swr_t *swr, value *in_vector,
                                        int offset) {
  CAMLparam0();
  CAMLlocal1(str);
  int str_len = caml_string_length(Field(*in_vector, 0));
  int i;
  int nb_samples = str_len / swr->in.bytes_per_samples - offset;

  if (nb_samples < 0)
    Fail("Invalid offset!");

  if (nb_samples > swr->in.nb_samples)
    alloc_data(&swr->in, nb_samples);

  for (i = 0; i < swr->in.nb_channels; i++) {
    str = Field(*in_vector, i);

    if (str_len != caml_string_length(str) - offset * swr->in.bytes_per_samples)
      Fail("Swresample failed to convert channel %d's %lu bytes : %d bytes "
           "were expected",
           i, caml_string_length(str), str_len);

    memcpy(swr->in.data[i],
           (uint8_t *)String_val(str) + offset * swr->in.bytes_per_samples,
           str_len);
  }
  CAMLreturnT(int, nb_samples);
}

static int get_in_samples_float_array(swr_t *swr, value *in_vector,
                                      int offset) {
  int i, linesize = Wosize_val(*in_vector) / Double_wosize;
  int nb_samples = linesize / swr->in.nb_channels - offset;

  if (nb_samples < 0)
    Fail("Invalid offset!");

  if (nb_samples > swr->in.nb_samples)
    alloc_data(&swr->in, nb_samples);

  double *pcm = (double *)swr->in.data[0];

  for (i = 0; i < linesize; i++) {
    pcm[i] = filter_nan(Double_field(*in_vector, i + offset));
  }

  return nb_samples;
}

static int get_in_samples_planar_float_array(swr_t *swr, value *in_vector,
                                             int offset) {
  CAMLparam0();
  CAMLlocal1(fa);
  int i, j, nb_words = Wosize_val(Field(*in_vector, 0));
  int nb_samples = nb_words / Double_wosize - offset;

  if (nb_samples < 0)
    Fail("Invalid offset!");

  if (nb_samples > swr->in.nb_samples)
    alloc_data(&swr->in, nb_samples);

  for (i = 0; i < swr->in.nb_channels; i++) {
    fa = Field(*in_vector, i);

    if (nb_words != Wosize_val(fa))
      Fail("Swresample failed to convert channel %d's %lu bytes : %d bytes "
           "were expected",
           i, Wosize_val(fa), nb_words);

    double *pcm = (double *)swr->in.data[i];

    for (j = 0; j < nb_samples; j++) {
      pcm[j] = filter_nan(Double_field(fa, j + offset));
    }
  }
  CAMLreturnT(int, nb_samples);
}

static int get_in_samples_ba(swr_t *swr, value *in_vector, int offset) {
  CAMLparam0();
  CAMLlocal1(ba);
  int nb_samples =
      Caml_ba_array_val(*in_vector)->dim[0] / swr->in.nb_channels - offset;

  if (nb_samples < 0)
    Fail("Invalid offset!");

  swr->in.data[0] = Caml_ba_data_val(*in_vector) + offset * swr->in.nb_channels;
  CAMLreturnT(int, nb_samples);
}

static int get_in_samples_planar_ba(swr_t *swr, value *in_vector, int offset) {
  CAMLparam0();
  CAMLlocal1(ba);
  int nb_samples = Caml_ba_array_val(Field(*in_vector, 0))->dim[0] - offset;

  if (nb_samples < 0)
    Fail("Invalid offset!");

  for (int i = 0; i < swr->in.nb_channels; i++) {
    ba = Field(*in_vector, i);

    if (nb_samples != Caml_ba_array_val(ba)->dim[0])
      Fail("Swresample failed to convert channel %d's %ld bytes : %d bytes "
           "were expected",
           i, Caml_ba_array_val(ba)->dim[0], nb_samples);

    swr->in.data[i] = Caml_ba_data_val(ba) + offset;
  }
  CAMLreturnT(int, nb_samples);
}

static void alloc_out_frame(swr_t *swr, int nb_samples) {
  CAMLparam0();
  CAMLlocal1(vector);
  int ret;

  AVFrame *frame = av_frame_alloc();

  if (!frame) {
    caml_raise_out_of_memory();
  }

  frame->nb_samples = nb_samples;
  frame->channel_layout = swr->out_channel_layout;
  frame->format = swr->out.sample_fmt;
  frame->sample_rate = swr->out_sample_rate;

  ret = av_frame_get_buffer(frame, 0);

  if (ret < 0) {
    av_frame_free(&frame);
    ocaml_avutil_raise_error(ret);
  }

  vector = value_of_frame(frame);
  caml_modify_generational_global_root(&swr->out_vector, vector);
  swr->out.data = frame->extended_data;
  swr->out.nb_samples = nb_samples;
}

static void convert_to_frame(swr_t *swr, int in_nb_samples,
                             int out_nb_samples) {
  alloc_out_frame(swr, out_nb_samples);

  caml_release_runtime_system();
  int ret = swr_convert(swr->context, swr->out.data, swr->out.nb_samples,
                        (const uint8_t **)swr->in.data, in_nb_samples);
  caml_acquire_runtime_system();

  if (ret < 0)
    ocaml_avutil_raise_error(ret);

  Frame_val(swr->out_vector)->nb_samples = ret;
}

static void convert_to_string(swr_t *swr, int in_nb_samples,
                              int out_nb_samples) {
  // Allocate out data if needed
  if (out_nb_samples > swr->out.nb_samples)
    alloc_data(&swr->out, out_nb_samples);

  caml_release_runtime_system();
  int ret = swr_convert(swr->context, swr->out.data, swr->out.nb_samples,
                        (const uint8_t **)swr->in.data, in_nb_samples);
  caml_acquire_runtime_system();

  if (ret < 0)
    ocaml_avutil_raise_error(ret);

  size_t len = ret * swr->out.nb_channels * swr->out.bytes_per_samples;

  caml_modify_generational_global_root(&swr->out_vector,
                                       caml_alloc_string(len));
  swr->out_vector_nb_samples = ret;

  memcpy(Bytes_val(swr->out_vector), swr->out.data[0], len);
}

static void convert_to_planar_string(swr_t *swr, int in_nb_samples,
                                     int out_nb_samples) {
  // Allocate out data if needed
  if (out_nb_samples > swr->out.nb_samples)
    alloc_data(&swr->out, out_nb_samples);

  caml_release_runtime_system();
  int ret = swr_convert(swr->context, swr->out.data, swr->out.nb_samples,
                        (const uint8_t **)swr->in.data, in_nb_samples);
  caml_acquire_runtime_system();

  if (ret < 0)
    ocaml_avutil_raise_error(ret);

  size_t len = ret * swr->out.bytes_per_samples;
  int i;

  for (i = 0; i < swr->out.nb_channels; i++) {
    Store_field(swr->out_vector, i, caml_alloc_string(len));
  }
  swr->out_vector_nb_samples = ret;

  for (i = 0; i < swr->out.nb_channels; i++) {
    memcpy(Bytes_val(Field(swr->out_vector, i)), swr->out.data[i], len);
  }
}

static void convert_to_float_array(swr_t *swr, int in_nb_samples,
                                   int out_nb_samples) {
  // Allocate out data if needed
  if (out_nb_samples > swr->out.nb_samples)
    alloc_data(&swr->out, out_nb_samples);

  caml_release_runtime_system();
  int ret = swr_convert(swr->context, swr->out.data, swr->out.nb_samples,
                        (const uint8_t **)swr->in.data, in_nb_samples);
  caml_acquire_runtime_system();

  if (ret < 0)
    ocaml_avutil_raise_error(ret);

  size_t len = ret * swr->out.nb_channels;
  int i;

  caml_modify_generational_global_root(
      &swr->out_vector, caml_alloc(len * Double_wosize, Double_array_tag));
  swr->out_vector_nb_samples = ret;

  double *pcm = (double *)swr->out.data[0];

  for (i = 0; i < len; i++) {
    Store_double_field(swr->out_vector, i, filter_nan(pcm[i]));
  }
}

static void convert_to_planar_float_array(swr_t *swr, int in_nb_samples,
                                          int out_nb_samples) {
  // Allocate out data if needed
  if (out_nb_samples > swr->out.nb_samples)
    alloc_data(&swr->out, out_nb_samples);

  caml_release_runtime_system();
  int ret = swr_convert(swr->context, swr->out.data, swr->out.nb_samples,
                        (const uint8_t **)swr->in.data, in_nb_samples);
  caml_acquire_runtime_system();

  if (ret < 0)
    ocaml_avutil_raise_error(ret);

  int i, j;
  double *pcm;

  for (i = 0; i < swr->out.nb_channels; i++) {
    Store_field(swr->out_vector, i,
                caml_alloc(ret * Double_wosize, Double_array_tag));
  }
  swr->out_vector_nb_samples = ret;

  for (i = 0; i < swr->out.nb_channels; i++) {
    pcm = (double *)swr->out.data[i];

    for (j = 0; j < ret; j++)
      Store_double_field(Field(swr->out_vector, i), j, filter_nan(pcm[j]));
  }
}

static void alloc_out_ba(swr_t *swr, int nb_samples) {
  enum caml_ba_kind ba_kind =
      bigarray_kind_of_AVSampleFormat(swr->out.sample_fmt);
  intnat out_size = nb_samples * swr->out.nb_channels;

  caml_modify_generational_global_root(
      &swr->out_vector,
      caml_ba_alloc(CAML_BA_C_LAYOUT | ba_kind, 1, NULL, &out_size));

  swr->out.data[0] = Caml_ba_data_val(swr->out_vector);
  swr->out.nb_samples = nb_samples;
}

static void convert_to_ba(swr_t *swr, int in_nb_samples, int out_nb_samples) {
  alloc_out_ba(swr, out_nb_samples);

  caml_release_runtime_system();
  int ret = swr_convert(swr->context, swr->out.data, swr->out.nb_samples,
                        (const uint8_t **)swr->in.data, in_nb_samples);
  caml_acquire_runtime_system();

  if (ret < 0)
    ocaml_avutil_raise_error(ret);

  Caml_ba_array_val(swr->out_vector)->dim[0] = ret * swr->out.nb_channels;
}

static void alloc_out_planar_ba(swr_t *swr, int nb_samples) {
  enum caml_ba_kind ba_kind =
      bigarray_kind_of_AVSampleFormat(swr->out.sample_fmt);
  intnat out_size = nb_samples;
  int i;

  for (i = 0; i < swr->out.nb_channels; i++) {
    Store_field(swr->out_vector, i,
                caml_ba_alloc(CAML_BA_C_LAYOUT | ba_kind, 1, NULL, &out_size));

    swr->out.data[i] = Caml_ba_data_val(Field(swr->out_vector, i));
  }
  swr->out.nb_samples = nb_samples;
}

static void convert_to_planar_ba(swr_t *swr, int in_nb_samples,
                                 int out_nb_samples) {
  alloc_out_planar_ba(swr, out_nb_samples);

  caml_release_runtime_system();
  int ret = swr_convert(swr->context, swr->out.data, swr->out.nb_samples,
                        (const uint8_t **)swr->in.data, in_nb_samples);
  caml_acquire_runtime_system();

  if (ret < 0)
    ocaml_avutil_raise_error(ret);

  int i;
  for (i = 0; i < swr->out.nb_channels; i++) {
    Caml_ba_array_val(Field(swr->out_vector, i))->dim[0] = ret;
  }
}

CAMLprim value ocaml_swresample_convert(value _ofs, value _len, value _swr,
                                        value _in_vector) {
  CAMLparam4(_ofs, _len, _swr, _in_vector);
  swr_t *swr = Swr_val(_swr);

  // consistency check between the input channels and the context ones
  if (swr->in.is_planar) {
    int in_nb_channels = Wosize_val(_in_vector);

    if (in_nb_channels != swr->in.nb_channels)
      Fail("Swresample failed to convert %d channels : %d channels were "
           "expected",
           in_nb_channels, swr->in.nb_channels);
  }

  caml_modify_generational_global_root(&swr->out_vector,
                                       caml_alloc(swr->out.nb_channels, 0));

  // acquisition of the input samples and the input number of samples per
  // channel
  int offset = 0;
  if (_ofs != Val_none) {
    offset = Int_val(Field(_ofs, 0));
  }

  int in_nb_samples = swr->get_in_samples(swr, &_in_vector, offset);
  if (in_nb_samples < 0)
    ocaml_avutil_raise_error(in_nb_samples);

  if (_len != Val_none) {
    int asked_nb_samples = Int_val(Field(_len, 0));
    if (in_nb_samples < asked_nb_samples) {
      Fail("Input vector too small!");
    }
    in_nb_samples = asked_nb_samples;
  }

  // Computation of the output number of samples per channel according to the
  // input ones
  int out_nb_samples = swr_get_out_samples(swr->context, in_nb_samples);

  // Resample and convert input data to output data
  swr->convert(swr, in_nb_samples, out_nb_samples);

  CAMLreturn(swr->out_vector);
}

CAMLprim value ocaml_swresample_flush(value _swr) {
  CAMLparam1(_swr);
  swr_t *swr = Swr_val(_swr);

  caml_modify_generational_global_root(&swr->out_vector,
                                       caml_alloc(swr->out.nb_channels, 0));

  // Computation of the output number of samples per channel according to the
  // input ones
  int out_nb_samples = swr_get_out_samples(swr->context, 0);

  // Resample and convert input data to output data
  swr->convert(swr, 0, out_nb_samples);

  CAMLreturn(swr->out_vector);
}

void swresample_free(swr_t *swr) {
  if (swr->context)
    swr_free(&swr->context);

  if (swr->in.data && swr->get_in_samples != get_in_samples_frame) {

    if (swr->in.owns_data)
      av_freep(&swr->in.data[0]);

    free(swr->in.data);
  }

  if (swr->out.data && swr->convert != convert_to_frame) {

    if (swr->out.owns_data)
      av_freep(&swr->out.data[0]);

    free(swr->out.data);
  }

  if (swr->out_vector) {
    caml_remove_generational_global_root(&swr->out_vector);
  }

  free(swr);
}

CAMLprim ocaml_swresample_finalize(value v) {
  CAMLparam1(v);
  swresample_free(Swr_val(v));
  CAMLreturn(Val_unit);
}

static struct custom_operations swr_ops = {
    "ocaml_swresample_context", custom_finalize_default,
    custom_compare_default,     custom_hash_default,
    custom_serialize_default,   custom_deserialize_default};

#define NB_OPTIONS_TYPES 3

static SwrContext *swresample_set_context(
    swr_t *swr, int64_t in_channel_layout, enum AVSampleFormat in_sample_fmt,
    int in_sample_rate, int64_t out_channel_layout,
    enum AVSampleFormat out_sample_fmt, int out_sample_rate, value options[]) {
  if (!swr->context && !(swr->context = swr_alloc()))
    caml_raise_out_of_memory();

  SwrContext *ctx = swr->context;

  if (in_channel_layout) {
    av_opt_set_channel_layout(ctx, "in_channel_layout", in_channel_layout, 0);

    swr->in.nb_channels = av_get_channel_layout_nb_channels(in_channel_layout);
  }

  if (in_sample_fmt != AV_SAMPLE_FMT_NONE) {
    av_opt_set_sample_fmt(ctx, "in_sample_fmt", in_sample_fmt, 0);

    swr->in.sample_fmt = in_sample_fmt;
  }

  if (in_sample_rate) {
    av_opt_set_int(ctx, "in_sample_rate", in_sample_rate, 0);
  }

  if (out_channel_layout) {
    av_opt_set_channel_layout(ctx, "out_channel_layout", out_channel_layout, 0);
    swr->out_channel_layout = out_channel_layout;
    swr->out.nb_channels =
        av_get_channel_layout_nb_channels(out_channel_layout);
  }

  if (out_sample_fmt != AV_SAMPLE_FMT_NONE) {
    av_opt_set_sample_fmt(ctx, "out_sample_fmt", out_sample_fmt, 0);
    swr->out.sample_fmt = out_sample_fmt;
  }

  if (out_sample_rate) {
    av_opt_set_int(ctx, "out_sample_rate", out_sample_rate, 0);
    swr->out_sample_rate = out_sample_rate;
  }

  int i, ret = 0;

  for (i = 0; options[i]; i++) {
    int64_t val = DitherType_val_no_raise(options[i]);

    // TODO: get rid of that!
    if (val != VALUE_NOT_FOUND) {
      ret = av_opt_set_int(ctx, "dither_method", val, 0);
    } else {
      val = Engine_val_no_raise(options[i]);

      if (val != VALUE_NOT_FOUND) {
        ret = av_opt_set_int(ctx, "resampler", val, 0);
      } else {
        val = FilterType_val_no_raise(options[i]);

        if (val != VALUE_NOT_FOUND) {
          ret = av_opt_set_int(ctx, "filter_type", val, 0);
        }
      }
    }

    if (ret != 0)
      ocaml_avutil_raise_error(ret);
  }

  // initialize the resampling context
  caml_release_runtime_system();
  ret = swr_init(ctx);
  caml_acquire_runtime_system();

  if (ret < 0)
    ocaml_avutil_raise_error(ret);

  return ctx;
}

swr_t *swresample_create(vector_kind in_vector_kind, int64_t in_channel_layout,
                         enum AVSampleFormat in_sample_fmt, int in_sample_rate,
                         vector_kind out_vector_kind,
                         int64_t out_channel_layout,
                         enum AVSampleFormat out_sample_fmt,
                         int out_sample_rate, value options[]) {
  swr_t *swr = (swr_t *)calloc(1, sizeof(swr_t));
  if (!swr) {
    caml_raise_out_of_memory();
  }

  SwrContext *ctx = swresample_set_context(
      swr, in_channel_layout, in_sample_fmt, in_sample_rate, out_channel_layout,
      out_sample_fmt, out_sample_rate, options);

  if (!ctx) {
    swresample_free(swr);
    caml_raise_out_of_memory();
  }

  if (in_vector_kind != Frm) {
    swr->in.data = (uint8_t **)calloc(swr->in.nb_channels, sizeof(uint8_t *));
    swr->in.is_planar = av_sample_fmt_is_planar(swr->in.sample_fmt);
  }
  swr->in.bytes_per_samples = av_get_bytes_per_sample(in_sample_fmt);

  swr->out_vector = Val_unit;

  if (out_vector_kind != Frm) {
    swr->out.data = (uint8_t **)calloc(swr->out.nb_channels, sizeof(uint8_t *));
    swr->out.is_planar = av_sample_fmt_is_planar(swr->out.sample_fmt);

    if (swr->out.is_planar) {
      swr->out_vector = caml_alloc(swr->out.nb_channels, 0);
    }
  }

  caml_register_generational_global_root(&swr->out_vector);

  swr->out.bytes_per_samples = av_get_bytes_per_sample(out_sample_fmt);

  switch (in_vector_kind) {
  case Frm:
    swr->get_in_samples = get_in_samples_frame;
    break;
  case Str:
    swr->get_in_samples = get_in_samples_string;
    break;
  case P_Str:
    swr->get_in_samples = get_in_samples_planar_string;
    break;
  case Fa:
    swr->get_in_samples = get_in_samples_float_array;
    break;
  case P_Fa:
    swr->get_in_samples = get_in_samples_planar_float_array;
    break;
  case Ba:
    swr->get_in_samples = get_in_samples_ba;
    break;
  case P_Ba:
    swr->get_in_samples = get_in_samples_planar_ba;
  }

  switch (out_vector_kind) {
  case Frm:
    swr->convert = convert_to_frame;
    break;
  case Str:
    swr->convert = convert_to_string;
    break;
  case P_Str:
    swr->convert = convert_to_planar_string;
    break;
  case Fa:
    swr->convert = convert_to_float_array;
    break;
  case P_Fa:
    swr->convert = convert_to_planar_float_array;
    break;
  case Ba:
    swr->convert = convert_to_ba;
    break;
  case P_Ba:
    swr->convert = convert_to_planar_ba;
  }
  return swr;
}

CAMLprim value ocaml_swresample_create(
    value _in_vector_kind, value _in_channel_layout, value _in_sample_fmt,
    value _in_sample_rate, value _out_vector_kind, value _out_channel_layout,
    value _out_sample_fmt, value _out_sample_rate, value _options) {
  CAMLparam5(_in_channel_layout, _in_sample_fmt, _out_channel_layout,
             _out_sample_fmt, _options);
  CAMLlocal1(ans);

  vector_kind in_vector_kind = Int_val(_in_vector_kind);
  int64_t in_channel_layout = ChannelLayout_val(_in_channel_layout);
  enum AVSampleFormat in_sample_fmt = SampleFormat_val(_in_sample_fmt);
  int in_sample_rate = Int_val(_in_sample_rate);
  vector_kind out_vector_kind = Int_val(_out_vector_kind);
  int64_t out_channel_layout = ChannelLayout_val(_out_channel_layout);
  enum AVSampleFormat out_sample_fmt = SampleFormat_val(_out_sample_fmt);
  int out_sample_rate = Int_val(_out_sample_rate);
  value options[NB_OPTIONS_TYPES + 1];
  int i;

  for (i = 0; i < Wosize_val(_options) && i < NB_OPTIONS_TYPES; i++)
    options[i] = Field(_options, i);

  options[i] = 0;

  swr_t *swr =
      swresample_create(in_vector_kind, in_channel_layout, in_sample_fmt,
                        in_sample_rate, out_vector_kind, out_channel_layout,
                        out_sample_fmt, out_sample_rate, options);

  ans = caml_alloc_custom(&swr_ops, sizeof(swr_t *), 0, 1);
  Swr_val(ans) = swr;

  CAMLreturn(ans);
}

CAMLprim value ocaml_swresample_create_byte(value *argv, int argn) {
  return ocaml_swresample_create(argv[0], argv[1], argv[2], argv[3], argv[4],
                                 argv[5], argv[6], argv[7], argv[8]);
}
