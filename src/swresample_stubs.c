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

#include <libswresample/swresample.h>

#include "avutil_stubs.h"


/***** Contexts *****/

typedef struct {
  SwrContext *context;
    int out_nb_channels;
  //  int64_t out_channel_layout;
  //  int out_sample_rate;
  enum AVSampleFormat out_sample_fmt;
  value out_str;
  int out_str_size;
  value out_ar;
  int out_ar_size;
  value out_ba;
  int out_ba_size;
  enum caml_ba_kind out_ba_kind;
  value out_planar_data_array;
  int out_planar_str_size;
  int out_planar_ar_size;
  int out_planar_ba_size;
  enum caml_ba_kind out_planar_ba_kind;
} swr_t;

#define Swr_val(v) (*(struct swr_t**)Data_custom_val(v))

void swresample_free(swr_t swr)
{
  if( ! swr) return;
 
  if(swr->context) swr_free(&swr->context);

  if(swr->out_str) caml_remove_global_root(&swr->out_str);
  if(swr->out_ar) caml_remove_global_root(&swr->out_ar);
  if(swr->out_ba) caml_remove_global_root(&swr->out_ba);
  if(swr->out_planar_data_array)
    caml_remove_global_root(&swr->out_planar_data_array);

  free(swr);
}

static void finalize_swresample(value v)
{
  swresample_free(Swr_val(v));
}

static struct custom_operations swr_ops =
  {
    "ocaml_swresample_context",
    finalize_swresample,
    custom_compare_default,
    custom_hash_default,
    custom_serialize_default,
    custom_deserialize_default
  };

static value value_of_swr(swr_t *swr)
{
  if (!swr)
    Raise(EXN_FAILURE, "Empty swresample");

  value ans = caml_alloc_custom(&swr_ops, sizeof(swr_t*), 0, 1);
  Swr_val(ans) = swr;
  return ans;
}

/**
 */
void swresample_set_context(swr_t * swr,
			    int64_t in_channel_layout, enum AVSampleFormat in_sample_fmt, int in_sample_rate,
			    int64_t out_channel_layout, enum AVSampleFormat out_sample_fmt, int out_sample_rate)
{
  if( ! swr->context) {
    swr->context = swr_alloc();
  }

  if ( ! swr->context) {
    Raise(EXN_FAILURE, "Could not allocate swresample context");
  }

  SwrContext *ctx = swr->context;

  if(in_channel_layout) {
    av_opt_set_channel_layout(ctx, "in_channel_layout", in_channel_layout, 0);
  }
  
  if(in_sample_fmt != AV_SAMPLE_FMT_NONE) {
    av_opt_set_sample_fmt(ctx, "in_sample_fmt", in_sample_fmt,  0);
  }

  if(in_sample_rate) {
    av_opt_set_int(ctx, "in_sample_rate", in_sample_rate, 0);
  }

  if(out_channel_layout) {
    av_opt_set_channel_layout(ctx, "out_channel_layout", out_channel_layout, 0);
    swr->out_nb_channels = av_get_channel_layout_nb_channels(out_channel_layout);
  }
  
  if(out_sample_fmt != AV_SAMPLE_FMT_NONE) {
    av_opt_set_sample_fmt(ctx, "out_sample_fmt", out_sample_fmt,  0);
    swr->out_sample_fmt = out_sample_fmt;
  }

  if(out_sample_rate) {
    av_opt_set_int(ctx, "out_sample_rate", out_sample_rate, 0);
  }

  // initialize the resampling context
  int ret = swr_init(ctx);
  
  if (ret < 0) {
    snprintf(error_msg, ERROR_MSG_SIZE, "Failed to initialize the resampling context : %s", av_err2str(ret));
    Raise(EXN_FAILURE, error_msg);
  }

  if(av_sample_fmt_is_planar(swr->out_sample_fmt)
     && (swr->out_planar_data_array == 0
	 || Wosize_val(out_planar_data_array) != swr->out_nb_channels)) {

    if( ! swr->out_planar_data_array)
      caml_register_global_root(&swr->out_planar_data_array);
    
    swr->out_planar_data_array = caml_alloc(swr->out_nb_channels, 0);
    swr->out_planar_str_size = 0;
    swr->out_planar_ar_size = 0;
    swr->out_planar_ba_size = 0;
  }
}

swr_t swresample_create(int64_t in_channel_layout, enum AVSampleFormat in_sample_fmt, int in_sample_rate,
			int64_t out_channel_layout, enum AVSampleFormat out_sample_fmt, int out_sample_rate)
{
  swr_t * swr = (swr_t*)calloc(1, sizeof(swr_t));

  if ( ! swr) {
    snprintf(error_msg, ERROR_MSG_SIZE, "Failed to allocate Swresample context");
    Raise(EXN_FAILURE, error_msg);
  }

  swr->out_ba_kind = -1;

  swresample_set_context(swr,
			 in_channel_layout, in_sample_fmt, in_sample_rate,
			 out_channel_layout, out_sample_fmt, out_sample_rate);

  return swr;
}

CAMLprim value ocaml_swresample_create(value _in_channel_layout, value _in_sample_fmt, value _in_sample_rate, value _out_channel_layout, value _out_sample_fmt, value _out_sample_rate)
{
  CAMLparam5(_in_channel_layout, _in_sample_fmt, _in_sample_rate, _out_channel_layout, _out_sample_fmt);
  CAMLxparam1(_out_sample_rate);
  CAMLlocal1(ans);

  int64_t in_channel_layout = ChannelLayout_val(_in_channel_layout);
  enum AVSampleFormat in_sample_fmt = SampleFormat_val(_in_sample_fmt);
  int in_sample_rate = Int_val(_in_sample_rate);
  int64_t out_channel_layout = ChannelLayout_val(_out_channel_layout);
  enum AVSampleFormat out_sample_fmt = SampleFormat_val(_out_sample_fmt);
  int out_sample_rate = Int_val(_out_sample_rate);

  swr_t * swr = swresample_create(in_channel_layout, in_sample_fmt, in_sample_rate,
				  out_channel_layout, out_sample_fmt, out_sample_rate);
  
  ans = value_of_swr(swr);
  
  CAMLreturn(ans);
}

CAMLprim value ocaml_swresample_create_byte(value *argv, int argn)
{
  return ocaml_swresample_create( argv[0], argv[1], argv[2], argv[3], argv[4], argv[5]);
}


/*
 * Find an upper bound on the number of samples per channel
 * that the next swr_convert call will output.
 */
static int get_out_samples(swr_t * swr, AVSampleFormat sample_fmt, int nb_samples)
{
  // initialize the resampler if necessary
  if(swr->out_sample_fmt != sample_fmt) {
    resampler_set_context(swr, 0, AV_SAMPLE_FMT_NONE, 0, 0, sample_fmt, 0);
  }

  // compute destination number of samples
  return swr_get_out_samples(swr->context, nb_samples);
}

CAMLprim value ocaml_swresample_to_string(value _swr, value _frame)
{
  CAMLparam2(_swr, _frame);
  swr_t *swr = Swr_val(_swr);
  AVFrame *frame = Frame_val(_frame);
  
  int out_nb_samples = get_out_samples(swr,
				       av_get_packed_sample_fmt(swr->out_sample_fmt),
				       frame->nb_samples);

  if (out_nb_samples != swr->out_str_size) {

    if( ! swr->out_str) caml_register_global_root(&swr->out_str);

    size_t len = out_nb_samples * swr->out_nb_channels *
      av_get_bytes_per_sample(swr->out_sample_fmt);

    swr->out_str = caml_alloc_string(len);
    swr->out_str_size = out_nb_samples;
  }

  char * out_buf = String_val(swr->out_str);

  // Convert the samples using the resampler.
  int ret = swr_convert(swr->context,
			(uint8_t **) &out_buf,
			out_nb_samples,
			(const uint8_t **) frame->extended_data,
			frame->nb_samples);

  if (ret < 0) {
    snprintf(error_msg, ERROR_MSG_SIZE, "Failed to convert input samples (error '%d')", ret);
    Raise(EXN_FAILURE, error_msg);
  }

  if(ret != out_nb_samples) {
    size_t len = ret * swr->out_nb_channels * av_get_bytes_per_sample(swr->out_sample_fmt);

    swr->out_str = caml_alloc_string(len);
    memcpy(String_val(swr->out_str), out_buf, len);
    swr->out_str_size = ret;
  }

  CAMLreturn(swr->out_str);
}

CAMLprim value ocaml_swresample_to_planar_string(value _swr, value _frame)
{
  CAMLparam2(_swr, _frame);
  CAMLlocal1(ans);
  swr_t *swr = Swr_val(_swr);
  AVFrame *frame = Frame_val(_frame);

  int out_nb_samples = get_out_samples(swr,
				       av_get_planar_sample_fmt(swr->out_sample_fmt),
				       frame->nb_samples);

  if (out_nb_samples != swr->out_planar_str_size) {
    size_t len = out_nb_samples * av_get_bytes_per_sample(swr->out_sample_fmt);

    for(int i = 0; i < swr->out_nb_channels; i++) {
      Store_field(swr->out_planar_data_array, i, caml_alloc_string(len));
    }
    swr->out_planar_str_size = out_nb_samples;
    swr->out_planar_ar_size = 0;
    swr->out_planar_ba_size = 0;
  }

  uint8_t *out_data_pointers[16];
  int max_channels = swr->out_nb_channels > 16 ? 16 : swr->out_nb_channels;

  for(int i = 0; i < max_channels; i++) {
    out_data_pointers[i] = (uint8_t *)String_val(Field(swr->out_planar_data_array, i));
  }

  // Convert the samples using the resampler.
  int ret = swr_convert(swr->context,
			out_data_pointers,
			out_nb_samples,
			(const uint8_t **) frame->extended_data,
			frame->nb_samples);

  if (ret < 0) {
    snprintf(error_msg, ERROR_MSG_SIZE, "Failed to convert input samples (error '%d')", ret);
    Raise(EXN_FAILURE, error_msg);
  }

  if(ret != out_nb_samples) {
    size_t len = ret * av_get_bytes_per_sample(swr->out_sample_fmt);

    for(int i = 0; i < swr->out_nb_channels; i++) {
      ans = caml_alloc_string(len);
      memcpy(String_val(ans), out_data_pointers[i], len);
      Store_field(swr->out_planar_data_array, i, ans);
    }
    swr->out_planar_str_size = ret;
  }

  CAMLreturn(swr->out_planar_data_array);
}

CAMLprim value ocaml_swresample_to_float_array(value _swr, value _frame)
{
  CAMLparam2(_swr, _frame);
  swr_t *swr = Swr_val(_swr);
  AVFrame *frame = Frame_val(_frame);
  
  int out_nb_samples = get_out_samples(swr, AV_SAMPLE_FMT_DBL, frame->nb_samples);
  
  if (out_nb_samples != swr->out_ar_size) {

    if( ! swr->out_ar) caml_register_global_root(&swr->out_ar);

    size_t size = out_nb_samples * swr->out_nb_channels * Double_wosize;

    swr->out_ar = caml_alloc(size, Double_array_tag);
    swr->out_ar_size = out_nb_samples;
  }

  double * out_buf = (double*)swr->out_ar;

  // Convert the samples using the resampler.
  int ret = swr_convert(swr->context,
			(uint8_t **) &out_buf,
			out_nb_samples,
			(const uint8_t **) frame->extended_data,
			frame->nb_samples);

  if (ret < 0) {
    snprintf(error_msg, ERROR_MSG_SIZE, "Failed to convert input samples (error '%d')", ret);
    Raise(EXN_FAILURE, error_msg);
  }

  if(ret != out_nb_samples) {
    size_t size = ret * swr->out_nb_channels;
    size_t len = size * av_get_bytes_per_sample(swr->out_sample_fmt);

    swr->out_ar = caml_alloc(size * Double_wosize, Double_array_tag);
    memcpy((void*)swr->out_ar, out_buf, len);
    swr->out_ar_size = ret;
  }

  CAMLreturn(swr->out_ar);
}

CAMLprim value ocaml_swresample_to_float_planar_array(value _swr, value _frame)
{
  CAMLparam2(_swr, _frame);
  CAMLlocal1(ans);
  swr_t *swr = Swr_val(_swr);
  AVFrame *frame = Frame_val(_frame);

  int out_nb_samples = get_out_samples(swr, AV_SAMPLE_FMT_DBLP, frame->nb_samples);

  if (out_nb_samples != swr->out_planar_ar_size) {
    size_t size = out_nb_samples * Double_wosize;

    for(int i = 0; i < swr->out_nb_channels; i++) {
      Store_field(swr->out_planar_data_array, i, caml_alloc(size, Double_array_tag));
    }
    swr->out_planar_ar_size = out_nb_samples;
    swr->out_planar_str_size = 0;
    swr->out_planar_ba_size = 0;
  }

  double * out_data_pointers[16];
  int max_channels = swr->out_nb_channels > 16 ? 16 : swr->out_nb_channels;

  for(int i = 0; i < max_channels; i++) {
    out_data_pointers[i] = (double*)Field(swr->out_planar_data_array, i);
  }

  // Convert the samples using the resampler.
  int ret = swr_convert(swr->context,
			(uint8_t **)out_data_pointers,
			out_nb_samples,
			(const uint8_t **) frame->extended_data,
			frame->nb_samples);

  if (ret < 0) {
    snprintf(error_msg, ERROR_MSG_SIZE, "Failed to convert input samples (error '%d')", ret);
    Raise(EXN_FAILURE, error_msg);
  }

  if(ret != out_nb_samples) {
    size_t size = ret * Double_wosize;
    size_t len = ret * av_get_bytes_per_sample(swr->out_sample_fmt);

    for(int i = 0; i < swr->out_nb_channels; i++) {
      ans = caml_alloc(size, Double_array_tag);
      memcpy((void*)ans, out_data_pointers[i], len);
      Store_field(swr->out_planar_data_array, i, ans);
    }
    swr->out_planar_ar_size = ret;
  }

  CAMLreturn(swr->out_planar_data_array);
}

/**
 * Convert the input auto samples into the output sample format.
 * The conversion happens on a per-frame basis.
 */
static value to_ba(swr_t *swr, AVFrame *frame, enum AVSampleFormat sample_fmt, enum caml_ba_kind ba_kind)
{
  int out_nb_samples = get_out_samples(swr, sample_fmt, frame->nb_samples);

  if (ba_kind != swr->out_ba_kind
      || out_nb_samples > swr->out_ba_size) {

    if( ! swr->out_ba) caml_register_global_root(&swr->out_ba);

    intnat out_size = out_nb_samples * swr->out_nb_channels;
    
    swr->out_ba = caml_ba_alloc(CAML_BA_C_LAYOUT | ba_kind, 1, NULL, &out_size);
    swr->out_ba_kind = ba_kind;
    swr->out_ba_size = out_nb_samples;
  }

  char * out_buf = Caml_ba_data_val(swr->out_ba);
  
  // Convert the samples using the resampler.
  //  caml_release_runtime_system();
  int ret = swr_convert(swr->context,
			(uint8_t **) &out_buf,
			swr->out_ba_size,
			(const uint8_t **)frame->extended_data,
			frame->nb_samples);
  //  caml_acquire_runtime_system();

  if (ret < 0) {
    snprintf(error_msg, ERROR_MSG_SIZE, "Failed to convert input samples (error '%d')", ret);
    Raise(EXN_FAILURE, error_msg);
  }

  Caml_ba_array_val(swr->out_ba)->dim[0] = ret * swr->out_nb_channels;

  return swr->out_ba;
}

CAMLprim value ocaml_swresample_to_uint8_ba(value _swr, value _frame) {
  CAMLparam2(_swr, _frame);
  CAMLreturn(to_ba(Swr_val(_swr), Frame_val(_frame), AV_SAMPLE_FMT_U8, CAML_BA_UINT8));
}

CAMLprim value ocaml_swresample_to_int16_ba(value _swr, value _frame) {
  CAMLparam2(_swr, _frame);
  CAMLreturn(to_ba(Swr_val(_swr), Frame_val(_frame), AV_SAMPLE_FMT_S16, CAML_BA_SINT16));
}

CAMLprim value ocaml_swresample_to_int32_ba(value _swr, value _frame) {
  CAMLparam2(_swr, _frame);
  CAMLreturn(to_ba(Swr_val(_swr), Frame_val(_frame), AV_SAMPLE_FMT_S32, CAML_BA_INT32));
}

CAMLprim value ocaml_swresample_to_float32_ba(value _swr, value _frame) {
  CAMLparam2(_swr, _frame);
  CAMLreturn(to_ba(Swr_val(_swr), Frame_val(_frame), AV_SAMPLE_FMT_FLT, CAML_BA_FLOAT32));
}

CAMLprim value ocaml_swresample_to_float64_ba(value _swr, value _frame) {
  CAMLparam2(_swr, _frame);
  CAMLreturn(to_ba(Swr_val(_swr), Frame_val(_frame), AV_SAMPLE_FMT_DBL, CAML_BA_FLOAT64));
}


static value to_planar_ba(swr_t *swr, AVFrame *frame, enum AVSampleFormat sample_fmt, enum caml_ba_kind ba_kind)
{
  int out_nb_samples = get_out_samples(swr, sample_fmt, frame->nb_samples);

  if (ba_kind != swr->out_planar_ba_kind
      || out_nb_samples > swr->out_planar_ba_size) {

    for(int i = 0; i < swr->out_nb_channels; i++) {
      Store_field(swr->out_planar_data_array, i,
		  caml_ba_alloc(CAML_BA_C_LAYOUT | ba_kind, 1, NULL, &out_nb_samples));
    }
    swr->out_planar_ba_kind = ba_kind;
    swr->out_planar_ba_size = out_nb_samples;
    swr->out_planar_str_size = 0;
    swr->out_planar_ar_size = 0;
  }

  uint8_t *out_data_pointers[16];
  int max_channels = swr->out_nb_channels > 16 ? 16 : swr->out_nb_channels;

  for(int i = 0; i < max_channels; i++) {
    out_data_pointers[i] = Caml_ba_data_val(Field(swr->out_planar_data_array, i));
  }

  // Convert the samples using the resampler.
  // caml_release_runtime_system();
  int ret = swr_convert(swr->context,
			out_data_pointers,
			swr->out_planar_ba_size,
			(const uint8_t **) frame->extended_data,
			frame->nb_samples);
  // caml_acquire_runtime_system();

  if (ret < 0) {
    snprintf(error_msg, ERROR_MSG_SIZE, "Failed to convert input samples (error '%d')", ret);
    Raise(EXN_FAILURE, error_msg);
  }

  for(int i = 0; i < max_channels; i++) {
    Caml_ba_array_val(Field(swr->out_planar_data_array, i))->dim[0] = ret;
  }

  return swr->out_planar_data_array;
}

CAMLprim value ocaml_swresample_to_uint8_planar_ba(value _swr, value _frame) {
  CAMLparam2(_swr, _frame);
  CAMLreturn(to_planar_ba(Swr_val(_swr), Frame_val(_frame), AV_SAMPLE_FMT_U8P, CAML_BA_UINT8));
}

CAMLprim value ocaml_swresample_to_int16_planar_ba(value _swr, value _frame) {
  CAMLparam2(_swr, _frame);
  CAMLreturn(to_planar_ba(Swr_val(_swr), Frame_val(_frame), AV_SAMPLE_FMT_S16P, CAML_BA_SINT16));
}

CAMLprim value ocaml_swresample_to_int32_planar_ba(value _swr, value _frame) {
  CAMLparam2(_swr, _frame);
  CAMLreturn(to_planar_ba(Swr_val(_swr), Frame_val(_frame), AV_SAMPLE_FMT_S32P, CAML_BA_INT32));
}

CAMLprim value ocaml_swresample_to_float32_planar_ba(value _swr, value _frame) {
  CAMLparam2(_swr, _frame);
  CAMLreturn(to_planar_ba(Swr_val(_swr), Frame_val(_frame), AV_SAMPLE_FMT_FLTP, CAML_BA_FLOAT32));
}

CAMLprim value ocaml_swresample_to_float64_planar_ba(value _swr, value _frame) {
  CAMLparam2(_swr, _frame);
  CAMLreturn(to_planar_ba(Swr_val(_swr), Frame_val(_frame), AV_SAMPLE_FMT_DBLP, CAML_BA_FLOAT64));
}

