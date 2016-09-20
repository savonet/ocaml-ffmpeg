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
  /*
  int64_t out_channel_layout;
  int out_nb_channels;
  int out_sample_rate;
  */
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
  if(swr->out_planar_data_array) caml_remove_global_root(&swr->out_planar_data_array);

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
void swresample_set(swr_t * swr,
		    int64_t in_channel_layout, enum AVSampleFormat in_sample_fmt, int in_sample_rate,
		    int64_t out_channel_layout, enum AVSampleFormat out_sample_fmt, int out_sample_rate)
{
  if(out_sample_fmt != AV_SAMPLE_FMT_NONE) {
  swr->out_sample_fmt = out_sample_fmt;
  }

  // create swresample context
  swr->context = swr_alloc_set_opts(swr->context,
				    out_channel_layout, out_sample_fmt, out_sample_rate,
				    in_channel_layout, in_sample_fmt, in_sample_rate,
				    0, NULL);         // log_offset, log_ctx
  if ( ! swr->context) {
    Raise(EXN_FAILURE, "Could not allocate swresample context");
  }

  // initialize the resampling context
  int ret = swr_init(swr->context);
  
  if (ret < 0) {
    snprintf(error_msg, ERROR_MSG_SIZE, "Failed to initialize the resampling context : %s", av_err2str(ret));
    Raise(EXN_FAILURE, error_msg);
  }

  if(av_sample_fmt_is_planar(swr->out_sample_fmt)
     && (swr->out_planar_data_array == 0
	 || Wosize_val(out_planar_data_array) != swr->out_nb_channels)) {
    swr->out_planar_data_array = caml_alloc(swr->out_nb_channels, 0);
    swr->out_planar_str_size = 0;
    swr->out_planar_ar_size = 0;
    swr->out_planar_ba_size = 0;
  }
  
  caml_register_global_root(&swr->out_str);
  caml_register_global_root(&swr->out_ar);
  caml_register_global_root(&swr->out_ba);
  caml_register_global_root(&swr->out_planar_data_array);
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
    /*
    swr->out_channel_layout = codec->channel_layout;
    swr->out_nb_channels = av_get_channel_layout_nb_channels(codec->channel_layout);
    swr->out_sample_rate = codec->sample_rate;
    swr->out_sample_fmt = swr->str_sample_fmt = codec->sample_fmt;

  if(channel_layout) {
    swr->out_channel_layout = channel_layout;
    swr->out_nb_channels = av_get_channel_layout_nb_channels(channel_layout);
  }

  if(sample_rate) {
    swr->out_sample_rate = sample_rate;
  }

  if(sample_fmt != AV_SAMPLE_FMT_NONE) {
    swr->out_sample_fmt = sample_fmt;
  }
    */  
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


CAMLprim value ocaml_swresample_create(value flags_, value src_w_, value src_h_, value src_format_, value dst_w_, value dst_h_, value dst_format_)
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
  struct SwrContext *c;

  for (i = 0; i < Wosize_val(flags_); i++)
    flags |= Flag_val(Field(flags_,i));

  caml_release_runtime_system();
  c = sws_getContext(src_w, src_h, src_format, dst_w, dst_h, dst_format, flags, NULL, NULL, NULL);
  caml_acquire_runtime_system();

  assert(c);
  ans = caml_alloc_custom(&swr_ops, sizeof(struct SwrContext*), 0, 1);
  Context_val(ans) = c;
  CAMLreturn(ans);
}

CAMLprim value ocaml_swresample_convert(value swr_, value src_, value off_, value h_, value dst_, value dst_off)
{
  CAMLparam4(swr_, src_, dst_, dst_off);
  CAMLlocal1(v);
  struct swr_t *swr = Swr_val(swr_);
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

/*
 * Find an upper bound on the number of samples per channel
 * that the next swr_convert call will output.
 */
static int get_out_samples(swr_t * swr, AVSampleFormat sample_fmt, int nb_samples)
{
  // initialize the resampler if necessary
  if(swr->out_sample_fmt != sample_fmt) {
    resampler_set(swr, 0, 0, sample_fmt);
  }
  
  // compute destination number of samples
  return swr_get_out_samples(swr->resampler, frame->nb_samples);
}

CAMLprim value ocaml_swresample_get_out_samples(value _sample_fmt, value _swr)
{
  CAMLparam2(_sample_fmt, _swr);

  swr_t *swr = Swr_val(_swr);

  if( ! av->audio_stream) open_audio_stream(av);

  stream_t * stream = av->audio_stream;
  enum AVSampleFormat sample_fmt = stream->audio->out_sample_fmt;

  if (Is_block(_sample_fmt)) {
    sample_fmt = SampleFormat_val(Field(_sample_fmt, 0));
  }

  CAMLreturn(Val_int(get_out_samples(stream, sample_fmt)));
}

CAMLprim value ocaml_swresample_to_string(value _swr, value _frame)
{
  CAMLparam2(_swr, _frame);
  swr_t *swr = Swr_val(_swr);
  AVFrame *frame = Frame_val(_frame);
  
  int out_nb_samples = get_out_samples(swr,
				       av_get_packed_sample_fmt(swr->str_sample_fmt),
				       frame->nb_samples);
  
  if (out_nb_samples != swr->out_str_size) {
    size_t len = out_nb_samples * audio->out_nb_channels *
      av_get_bytes_per_sample(audio->out_sample_fmt);

    swr->out_str = caml_alloc_string(len);
    swr->out_str_size = out_nb_samples;
  }

  char * out_buf = String_val(swr->out_str);

  // Convert the samples using the resampler.
  int ret = swr_convert(audio->resampler,
			(uint8_t **) &out_buf,
			out_nb_samples,
			(const uint8_t **) frame->extended_data,
			frame->nb_samples);

  if (ret < 0) {
    snprintf(error_msg, ERROR_MSG_SIZE, "Failed to convert input samples (error '%d')", ret);
    Raise(EXN_FAILURE, error_msg);
  }

  if(ret != out_nb_samples) {
    size_t len = ret * audio->out_nb_channels * av_get_bytes_per_sample(audio->out_sample_fmt);

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
				       av_get_planar_sample_fmt(audio->str_sample_fmt),
				       frame->nb_samples);

  if (out_nb_samples != swr->out_planar_str_size) {
    size_t len = out_nb_samples * av_get_bytes_per_sample(audio->out_sample_fmt);

    for(int i = 0; i < audio->out_nb_channels; i++) {
      Store_field(swr->out_planar_data_array, i, caml_alloc_string(len));
    }
    swr->out_planar_str_size = out_nb_samples;
    swr->out_planar_ar_size = 0;
    swr->out_planar_ba_size = 0;
  }

  uint8_t *out_data_pointers[16];
  int max_channels = audio->out_nb_channels > 16 ? 16 : audio->out_nb_channels;

  for(int i = 0; i < max_channels; i++) {
    out_data_pointers[i] = (uint8_t *)String_val(Field(swr->out_planar_data_array, i));
  }

  // Convert the samples using the resampler.
  int ret = swr_convert(audio->resampler,
			out_data_pointers,
			out_nb_samples,
			(const uint8_t **) frame->extended_data,
			frame->nb_samples);

  if (ret < 0) {
    snprintf(error_msg, ERROR_MSG_SIZE, "Failed to convert input samples (error '%d')", ret);
    Raise(EXN_FAILURE, error_msg);
  }

  if(ret != out_nb_samples) {
    size_t len = ret * av_get_bytes_per_sample(audio->out_sample_fmt);

    for(int i = 0; i < audio->out_nb_channels; i++) {
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
    size_t size = out_nb_samples * audio->out_nb_channels * Double_wosize;

    swr->out_ar = caml_alloc(size, Double_array_tag);
    swr->out_ar_size = out_nb_samples;
  }

  double * out_buf = (double*)swr->out_ar;

  // Convert the samples using the resampler.
  int ret = swr_convert(audio->resampler,
			(uint8_t **) &out_buf,
			out_nb_samples,
			(const uint8_t **) frame->extended_data,
			frame->nb_samples);

  if (ret < 0) {
    snprintf(error_msg, ERROR_MSG_SIZE, "Failed to convert input samples (error '%d')", ret);
    Raise(EXN_FAILURE, error_msg);
  }

  if(ret != out_nb_samples) {
    size_t size = ret * audio->out_nb_channels;
    size_t len = size * av_get_bytes_per_sample(audio->out_sample_fmt);

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

    for(int i = 0; i < audio->out_nb_channels; i++) {
      Store_field(swr->out_planar_data_array, i, caml_alloc(size, Double_array_tag));
    }
    swr->out_planar_ar_size = out_nb_samples;
    swr->out_planar_str_size = 0;
    swr->out_planar_ba_size = 0;
  }

  double * out_data_pointers[16];
  int max_channels = audio->out_nb_channels > 16 ? 16 : audio->out_nb_channels;

  for(int i = 0; i < max_channels; i++) {
    out_data_pointers[i] = (double*)Field(swr->out_planar_data_array, i);
  }

  // Convert the samples using the resampler.
  int ret = swr_convert(audio->resampler,
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
    size_t len = ret * av_get_bytes_per_sample(audio->out_sample_fmt);

    for(int i = 0; i < audio->out_nb_channels; i++) {
      ans = caml_alloc(size, Double_array_tag);
      memcpy((void*)ans, out_data_pointers[i], len);
      Store_field(swr->out_planar_data_array, i, ans);
    }
    swr->out_planar_ar_size = ret;
  }

  CAMLreturn(swr->out_planar_data_array);
}

/**
 * Convert the input audio samples into the output sample format.
 * The conversion happens on a per-frame basis.
 */
static value to_ba(swr_t *swr, AVFrame *frame, enum AVSampleFormat sample_fmt, enum caml_ba_kind ba_kind)
{
  int out_nb_samples = get_out_samples(swr, sample_fmt, frame->nb_samples);

  if (ba_kind != swr->out_ba_kind
      || out_nb_samples > swr->out_ba_size) {

    intnat out_size = out_nb_samples * audio->out_nb_channels;
    
    swr->out_ba = caml_ba_alloc(CAML_BA_C_LAYOUT | ba_kind, 1, NULL, &out_size);
    swr->out_ba_kind = ba_kind;
    swr->out_ba_size = out_nb_samples;
  }

  char * out_buf = Caml_ba_data_val(swr->out_ba);
  
  // Convert the samples using the resampler.
  caml_release_runtime_system();
  int ret = swr_convert(audio->resampler,
			(uint8_t **) &out_buf,
			swr->out_ba_size,
			(const uint8_t **)frame->extended_data,
			frame->nb_samples);
  caml_acquire_runtime_system();

  if (ret < 0) {
    snprintf(error_msg, ERROR_MSG_SIZE, "Failed to convert input samples (error '%d')", ret);
    Raise(EXN_FAILURE, error_msg);
  }

  Caml_ba_array_val(swr->out_ba)->dim[0] = ret * audio->out_nb_channels;

  return swr->out_ba;
}

CAMLprim value ocaml_swresample_to_uint8_ba(value _swr, value _frame) {
  CAMLparam2(_swr, _frame);
  CAMLreturn(to_ba(Swr_val(_swr), Frame(_frame), AV_SAMPLE_FMT_U8, CAML_BA_UINT8));
}

CAMLprim value ocaml_swresample_to_int16_ba(value _swr, value _frame) {
  CAMLparam2(_swr, _frame);
  CAMLreturn(to_ba(Swr_val(_swr), Frame(_frame), AV_SAMPLE_FMT_S16, CAML_BA_SINT16));
}

CAMLprim value ocaml_swresample_to_int32_ba(value _swr, value _frame) {
  CAMLparam2(_swr, _frame);
  CAMLreturn(to_ba(Swr_val(_swr), Frame(_frame), AV_SAMPLE_FMT_S32, CAML_BA_INT32));
}

CAMLprim value ocaml_swresample_to_float32_ba(value _swr, value _frame) {
  CAMLparam2(_swr, _frame);
  CAMLreturn(to_ba(Swr_val(_swr), Frame(_frame), AV_SAMPLE_FMT_FLT, CAML_BA_FLOAT32));
}

CAMLprim value ocaml_swresample_to_float64_ba(value _swr, value _frame) {
  CAMLparam2(_swr, _frame);
  CAMLreturn(to_ba(Swr_val(_swr), Frame(_frame), AV_SAMPLE_FMT_DBL, CAML_BA_FLOAT64));
}


static value to_planar_ba(swr_t *swr, AVFrame *frame, enum AVSampleFormat sample_fmt, enum caml_ba_kind ba_kind)
{
  int out_nb_samples = get_out_samples(swr, sample_fmt, frame->nb_samples);

  if (ba_kind != swr->out_planar_ba_kind
      || out_nb_samples > swr->out_planar_ba_size) {

    for(int i = 0; i < audio->out_nb_channels; i++) {
      Store_field(swr->out_planar_data_array, i,
		  caml_ba_alloc(CAML_BA_C_LAYOUT | ba_kind, 1, NULL, &out_nb_samples));
    }
    swr->out_planar_ba_kind = ba_kind;
    swr->out_planar_ba_size = out_nb_samples;
    swr->out_planar_str_size = 0;
    swr->out_planar_ar_size = 0;
  }

  uint8_t *out_data_pointers[16];
  int max_channels = audio->out_nb_channels > 16 ? 16 : audio->out_nb_channels;

  for(int i = 0; i < max_channels; i++) {
    out_data_pointers[i] = Caml_ba_data_val(Field(swr->out_planar_data_array, i));
  }

  // Convert the samples using the resampler.
  // caml_release_runtime_system();
  int ret = swr_convert(audio->resampler,
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
  CAMLreturn(to_planar_ba(Swr_val(_swr), Frame(_frame), AV_SAMPLE_FMT_U8P, CAML_BA_UINT8));
}

CAMLprim value ocaml_swresample_to_int16_planar_ba(value _swr, value _frame) {
  CAMLparam2(_swr, _frame);
  CAMLreturn(to_planar_ba(Swr_val(_swr), Frame(_frame), AV_SAMPLE_FMT_S16P, CAML_BA_SINT16));
}

CAMLprim value ocaml_swresample_to_int32_planar_ba(value _swr, value _frame) {
  CAMLparam2(_swr, _frame);
  CAMLreturn(to_planar_ba(Swr_val(_swr), Frame(_frame), AV_SAMPLE_FMT_S32P, CAML_BA_INT32));
}

CAMLprim value ocaml_swresample_to_float32_planar_ba(value _swr, value _frame) {
  CAMLparam2(_swr, _frame);
  CAMLreturn(to_planar_ba(Swr_val(_swr), Frame(_frame), AV_SAMPLE_FMT_FLTP, CAML_BA_FLOAT32));
}

CAMLprim value ocaml_swresample_to_float64_planar_ba(value _swr, value _frame) {
  CAMLparam2(_swr, _frame);
  CAMLreturn(to_planar_ba(Swr_val(_swr), Frame(_frame), AV_SAMPLE_FMT_DBLP, CAML_BA_FLOAT64));
}


