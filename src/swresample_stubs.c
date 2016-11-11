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

#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>
#include <libavutil/imgutils.h>
#include <libavutil/timestamp.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>

#include <avutil_stubs.h>
#include <swresample_stubs.h>

#define ERROR_MSG_SIZE 256
static char error_msg[ERROR_MSG_SIZE + 1];


/***** Contexts *****/

struct swr_t {
  SwrContext *context;
  uint8_t **in_data;
  int in_vector_is_array;
  int in_nb_channels;
  enum AVSampleFormat in_sample_fmt;
  int in_bytes_per_samples;
  uint8_t **out_data;
  int out_nb_channels;
  int64_t out_channel_layout;
  int out_sample_rate;
  enum AVSampleFormat out_sample_fmt;
  value out_vector;
  int out_size;
  int out_buf_size;

  int (*get_in_samples)(swr_t *, value *);
  void (*set_in_data)(swr_t *, value *);
  void (*alloc_out_vector)(swr_t *, int);
  int (*convert)(swr_t *, uint8_t **, int, int);
};

#define Swr_val(v) (*(swr_t**)Data_custom_val(v))


static int get_in_samples_frame(swr_t *swr, value * in_vector)
{
  return Frame_val(*in_vector)->nb_samples;
}

static void set_in_data_frame(swr_t *swr, value *in_vector)
{
  AVFrame *frame = Frame_val(*in_vector);
  int nb_channels = av_frame_get_channels(frame);
    
  if (nb_channels != swr->in_nb_channels) {
    snprintf(error_msg, ERROR_MSG_SIZE, "Swresample failed to convert %d channels : %d channels were expected", nb_channels, swr->in_nb_channels);
    Raise(EXN_FAILURE, error_msg);
  }

  if (frame->format != swr->in_sample_fmt) {
    snprintf(error_msg, ERROR_MSG_SIZE, "Swresample failed to convert %s sample format : %s sample format were expected", av_get_sample_fmt_name(frame->format), av_get_sample_fmt_name(swr->in_sample_fmt));
    Raise(EXN_FAILURE, error_msg);
  }

  swr->in_data = frame->extended_data;
}

static int get_in_samples_string(swr_t *swr, value * in_vector)
{
  return caml_string_length(*in_vector) / swr->in_bytes_per_samples;
}

static void set_in_data_string(swr_t *swr, value *in_vector)
{
  swr->in_data[0] = (uint8_t*)String_val(*in_vector);
}

static int get_in_samples_planar_string(swr_t *swr, value * in_vector)
{
  return caml_string_length(Field(*in_vector, 0)) / swr->in_bytes_per_samples;
}

static void set_in_data_planar_string(swr_t *swr, value *in_vector)
{
  CAMLparam0();
  CAMLlocal1(str);
  str = Field(*in_vector, 0);
  int str_len = caml_string_length(str);

  swr->in_data[0] = (uint8_t*)String_val(str);
    
  for (int i = 1; i < swr->in_nb_channels; i++) {
    str = Field(*in_vector, i);
    
    if(str_len != caml_string_length(str)) {
      snprintf(error_msg, ERROR_MSG_SIZE, "Swresample failed to convert channel %d's %lu bytes : %d bytes were expected", i, caml_string_length(str), str_len);
      Raise(EXN_FAILURE, error_msg);
    }
    swr->in_data[i] = (uint8_t*)String_val(str);
  }
  CAMLreturn0;
}

static int get_in_samples_float_array(swr_t *swr, value * in_vector)
{
  return Wosize_val(*in_vector) / (Double_wosize * swr->in_nb_channels);
}

static void set_in_data_float_array(swr_t *swr, value *in_vector)
{
  swr->in_data[0] = (uint8_t*)(*in_vector);
}

static int get_in_samples_planar_float_array(swr_t *swr, value *in_vector)
{
  return Wosize_val(Field(*in_vector, 0)) / Double_wosize;
}

static void set_in_data_planar_float_array(swr_t *swr, value *in_vector)
{
  CAMLparam0();
  CAMLlocal1(fa);
  fa = Field(*in_vector, 0);
  int fa_len = Wosize_val(fa);

  swr->in_data[0] = (uint8_t*)fa;

  for (int i = 1; i < swr->in_nb_channels; i++) {
    fa = Field(*in_vector, i);
    
    if(fa_len != Wosize_val(fa)) {
      snprintf(error_msg, ERROR_MSG_SIZE, "Swresample failed to convert channel %d's %lu bytes : %d bytes were expected", i, Wosize_val(fa), fa_len);
      Raise(EXN_FAILURE, error_msg);
    }
    swr->in_data[i] = (uint8_t*)fa;
  }
  CAMLreturn0;
}

static int get_in_samples_ba(swr_t *swr, value *in_vector)
{
  return Caml_ba_array_val(*in_vector)->dim[0] / swr->in_nb_channels;
}

static void set_in_data_ba(swr_t *swr, value *in_vector)
{
  swr->in_data[0] = Caml_ba_data_val(*in_vector);
}

static int get_in_samples_planar_ba(swr_t *swr, value *in_vector)
{
  return Caml_ba_array_val(Field(*in_vector, 0))->dim[0];
}

static void set_in_data_planar_ba(swr_t *swr, value *in_vector)
{
  CAMLparam0();
  CAMLlocal1(ba);
  ba = Field(*in_vector, 0);
  int in_nb_samples = Caml_ba_array_val(ba)->dim[0];

  swr->in_data[0] = Caml_ba_data_val(ba);
    
  for (int i = 1; i < swr->in_nb_channels; i++) {
    ba = Field(*in_vector, i);
    
    if(in_nb_samples != Caml_ba_array_val(ba)->dim[0]) {
      snprintf(error_msg, ERROR_MSG_SIZE, "Swresample failed to convert channel %d's %ld bytes : %d bytes were expected", i, Caml_ba_array_val(ba)->dim[0], in_nb_samples);
      Raise(EXN_FAILURE, error_msg);
    }
    swr->in_data[i] = Caml_ba_data_val(ba);
  }
  CAMLreturn0;
}


static void alloc_out_frame(swr_t *swr, int out_nb_samples)
{
  CAMLparam0();
  CAMLlocal1(vector);
  
  if (out_nb_samples > swr->out_buf_size) {

    AVFrame * frame = av_frame_alloc();

    if ( ! frame) {
      snprintf(error_msg, ERROR_MSG_SIZE, "Failed to allocate resampling output frame");
      Raise(EXN_FAILURE, error_msg);
    }

    frame->nb_samples     = out_nb_samples;
    frame->channel_layout = swr->out_channel_layout;
    frame->format         = swr->out_sample_fmt;
    frame->sample_rate    = swr->out_sample_rate;

    int ret = av_frame_get_buffer(frame, 0);

    if (ret < 0) {
      av_frame_free(&frame);
      snprintf(error_msg, ERROR_MSG_SIZE, "Failed to allocate resampling output frame samples : %s", av_err2str(ret));
      Raise(EXN_FAILURE, error_msg);
    }

    value_of_frame(frame, &vector);
    caml_modify_generational_global_root(&swr->out_vector, vector);
    swr->out_buf_size = out_nb_samples;
  }
  CAMLreturn0;
}

static int convert_to_frame(swr_t *swr, uint8_t **in_data, int in_nb_samples, int out_nb_samples)
{
  AVFrame *frame = Frame_val(swr->out_vector);

  // Convert the samples.
  int ret = swr_convert(swr->context,
			frame->extended_data,
			swr->out_buf_size,
			(const uint8_t **) in_data,
			in_nb_samples);

  frame->nb_samples = ret;
  return ret;
}

static void alloc_out_string(swr_t *swr, int out_nb_samples)
{
  size_t len = out_nb_samples * swr->out_nb_channels *
    av_get_bytes_per_sample(swr->out_sample_fmt);

  caml_modify_generational_global_root(&swr->out_vector, caml_alloc_string(len));
}

static int convert_to_string(swr_t *swr, uint8_t **in_data, int in_nb_samples, int out_nb_samples)
{
  CAMLparam0();
  CAMLlocal1(vector);
  char * out_data = String_val(swr->out_vector);

  // Convert the samples.
  int ret = swr_convert(swr->context,
			(uint8_t **) &out_data,
			out_nb_samples,
			(const uint8_t **) in_data,
			in_nb_samples);

  if(ret > 0 && ret != out_nb_samples) {
    size_t len = ret * swr->out_nb_channels * av_get_bytes_per_sample(swr->out_sample_fmt);

    vector = caml_alloc_string(len);
    memcpy(String_val(vector), String_val(swr->out_vector), len);
    caml_modify_generational_global_root(&swr->out_vector, vector);
    swr->out_size = ret;
  }
  CAMLreturnT(int, ret);
}

static void alloc_out_planar_string(swr_t *swr, int out_nb_samples)
{
  size_t len = out_nb_samples * av_get_bytes_per_sample(swr->out_sample_fmt);

  for(int i = 0; i < swr->out_nb_channels; i++) {
    Store_field(swr->out_vector, i, caml_alloc_string(len));
  }
}

static int convert_to_planar_string(swr_t *swr, uint8_t **in_data, int in_nb_samples, int out_nb_samples)
{
  CAMLparam0();
  CAMLlocal1(channel);
  
  for(int i = 0; i < swr->out_nb_channels; i++) {
    swr->out_data[i] = (uint8_t *)String_val(Field(swr->out_vector, i));
  }

  // Convert the samples.
  int ret = swr_convert(swr->context,
			swr->out_data,
			out_nb_samples,
			(const uint8_t **) in_data,
			in_nb_samples);

  if(ret > 0 && ret != out_nb_samples) {
    size_t len = ret * av_get_bytes_per_sample(swr->out_sample_fmt);

    for(int i = 0; i < swr->out_nb_channels; i++) {
      channel = caml_alloc_string(len);
      memcpy(String_val(channel), String_val(Field(swr->out_vector, i)), len);
      Store_field(swr->out_vector, i, channel);
    }
    swr->out_size = ret;
  }
  CAMLreturnT(int, ret);
}

static void alloc_out_float_array(swr_t *swr, int out_nb_samples)
{
  size_t size = out_nb_samples * swr->out_nb_channels * Double_wosize;

  caml_modify_generational_global_root(&swr->out_vector, caml_alloc(size, Double_array_tag));
}

static int convert_to_float_array(swr_t *swr, uint8_t **in_data, int in_nb_samples, int out_nb_samples)
{
  CAMLparam0();
  CAMLlocal1(vector);
  uint8_t *out_data = (uint8_t *)swr->out_vector;

  // Convert the samples.
  int ret = swr_convert(swr->context,
			&out_data,
			out_nb_samples,
			(const uint8_t **) in_data,
			in_nb_samples);

  if(ret > 0 && ret != out_nb_samples) {
    size_t size = ret * swr->out_nb_channels;
    size_t len = size * av_get_bytes_per_sample(swr->out_sample_fmt);

    vector = caml_alloc(size * Double_wosize, Double_array_tag);
    memcpy((void*)vector, (uint8_t *)swr->out_vector, len);
    caml_modify_generational_global_root(&swr->out_vector, vector);
    swr->out_size = ret;
  }
  CAMLreturnT(int, ret);
}

static void alloc_out_planar_float_array(swr_t *swr, int out_nb_samples)
{
  size_t size = out_nb_samples * Double_wosize;

  for(int i = 0; i < swr->out_nb_channels; i++) {
    Store_field(swr->out_vector, i, caml_alloc(size, Double_array_tag));
  }
}

static int convert_to_planar_float_array(swr_t *swr, uint8_t **in_data, int in_nb_samples, int out_nb_samples)
{
  CAMLparam0();
  CAMLlocal1(channel);

  for(int i = 0; i < swr->out_nb_channels; i++) {
    swr->out_data[i] = (uint8_t*)Field(swr->out_vector, i);
  }

  // Convert the samples.
  int ret = swr_convert(swr->context,
			swr->out_data,
			out_nb_samples,
			(const uint8_t **) in_data,
			in_nb_samples);

  if(ret > 0 && ret != out_nb_samples) {
    size_t size = ret * Double_wosize;
    size_t len = ret * av_get_bytes_per_sample(swr->out_sample_fmt);

    for(int i = 0; i < swr->out_nb_channels; i++) {
      channel = caml_alloc(size, Double_array_tag);
      memcpy((void*)channel, (void*)Field(swr->out_vector, i), len);
      Store_field(swr->out_vector, i, channel);
    }
    swr->out_size = ret;
  }
  CAMLreturnT(int, ret);
}

static void alloc_out_ba(swr_t *swr, int out_nb_samples)
{
  if (out_nb_samples > swr->out_buf_size) {
    enum caml_ba_kind ba_kind = bigarray_kind_of_Sample_format(swr->out_sample_fmt);
    intnat out_size = out_nb_samples * swr->out_nb_channels;

    caml_modify_generational_global_root(&swr->out_vector,
					 caml_ba_alloc(CAML_BA_C_LAYOUT | ba_kind, 1, NULL, &out_size));
    swr->out_buf_size = out_nb_samples;
  }
}

/**
 * Convert the input auto samples into the output sample format.
 * The conversion happens on a per-frame basis.
 */
static int convert_to_ba(swr_t *swr, uint8_t **in_data, int in_nb_samples, int out_nb_samples)
{
  char * out_data = Caml_ba_data_val(swr->out_vector);
  
  // Convert the samples.
  int ret = swr_convert(swr->context,
			(uint8_t **) &out_data,
			swr->out_buf_size,
			(const uint8_t **)in_data,
			in_nb_samples);

  Caml_ba_array_val(swr->out_vector)->dim[0] = ret * swr->out_nb_channels;
  return ret;
}


static void alloc_out_planar_ba(swr_t *swr, int out_nb_samples)
{
  if (out_nb_samples > swr->out_buf_size) {
    enum caml_ba_kind ba_kind = bigarray_kind_of_Sample_format(swr->out_sample_fmt);
    intnat out_size = out_nb_samples;

    for(int i = 0; i < swr->out_nb_channels; i++) {
      Store_field(swr->out_vector, i,
		  caml_ba_alloc(CAML_BA_C_LAYOUT | ba_kind, 1, NULL, &out_size));
    }
    swr->out_buf_size = out_nb_samples;
  }
}


static int convert_to_planar_ba(swr_t *swr, uint8_t **in_data, int in_nb_samples, int out_nb_samples)
{
  for(int i = 0; i < swr->out_nb_channels; i++) {
    swr->out_data[i] = Caml_ba_data_val(Field(swr->out_vector, i));
  }

  // Convert the samples.
  int ret = swr_convert(swr->context,
			swr->out_data,
			swr->out_buf_size,
			(const uint8_t **) in_data,
			in_nb_samples);

  for(int i = 0; i < swr->out_nb_channels; i++) {
    Caml_ba_array_val(Field(swr->out_vector, i))->dim[0] = ret;
  }
  return ret;
}

CAMLprim value ocaml_swresample_convert(value _swr, value _in_vector)
{
  CAMLparam2(_swr, _in_vector);
  swr_t *swr = Swr_val(_swr);

  // consistency check between the input channels and the context ones
  if(swr->in_vector_is_array) {
    int in_nb_channels = Wosize_val(_in_vector);

    if (in_nb_channels != swr->in_nb_channels) {
      snprintf(error_msg, ERROR_MSG_SIZE, "Swresample failed to convert %d channels : %d channels were expected", in_nb_channels, swr->in_nb_channels);
      Raise(EXN_FAILURE, error_msg);
    }
  }

  // Acquisition of the input number of samples per channel
  int in_nb_samples = swr->get_in_samples(swr, &_in_vector);

  // Computation of the output number of samples per channel according to the input ones
  int out_nb_samples = swr_get_out_samples(swr->context, in_nb_samples);

  // Allocate out_vector by calling alloc_out_vector function if needed
  if (out_nb_samples != swr->out_size) {
    swr->alloc_out_vector(swr, out_nb_samples);
    swr->out_size = out_nb_samples;
  }

  // Assignment of the input data
  swr->set_in_data(swr, &_in_vector);
  // No ocaml allocation should be made between set_in_data and convert to not invalidate the input data

  // Resample and convert input data to output data
  int ret = swr->convert(swr, swr->in_data, in_nb_samples, out_nb_samples);

  if (ret < 0) {
    snprintf(error_msg, ERROR_MSG_SIZE, "Failed to convert input samples (error '%d')", ret);
    Raise(EXN_FAILURE, error_msg);
  }

  CAMLreturn(swr->out_vector);
}

/**
 */
static void swresample_set_context(swr_t * swr,
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

    swr->in_nb_channels = av_get_channel_layout_nb_channels(in_channel_layout);
  }
  
  if(in_sample_fmt != AV_SAMPLE_FMT_NONE) {
    av_opt_set_sample_fmt(ctx, "in_sample_fmt", in_sample_fmt,  0);

    swr->in_sample_fmt = in_sample_fmt;
  }

  if(in_sample_rate) {
    av_opt_set_int(ctx, "in_sample_rate", in_sample_rate, 0);
  }

  if(out_channel_layout) {
    av_opt_set_channel_layout(ctx, "out_channel_layout", out_channel_layout, 0);
    swr->out_channel_layout = out_channel_layout;
    swr->out_nb_channels = av_get_channel_layout_nb_channels(out_channel_layout);
  }
  
  if(out_sample_fmt != AV_SAMPLE_FMT_NONE) {
    av_opt_set_sample_fmt(ctx, "out_sample_fmt", out_sample_fmt,  0);
    swr->out_sample_fmt = out_sample_fmt;
  }

  if(out_sample_rate) {
    av_opt_set_int(ctx, "out_sample_rate", out_sample_rate, 0);
    swr->out_sample_rate = out_sample_rate;
  }

  // initialize the resampling context
  int ret = swr_init(ctx);
  
  if (ret < 0) {
    snprintf(error_msg, ERROR_MSG_SIZE, "Failed to initialize the resampling context : %s", av_err2str(ret));
    Raise(EXN_FAILURE, error_msg);
  }
}

swr_t * swresample_create(vector_kind in_vector_kind, int64_t in_channel_layout, enum AVSampleFormat in_sample_fmt, int in_sample_rate,
			vector_kind out_vector_kind, int64_t out_channel_layout, enum AVSampleFormat out_sample_fmt, int out_sample_rate)
{
  swr_t * swr = (swr_t*)calloc(1, sizeof(swr_t));

  if ( ! swr) {
    snprintf(error_msg, ERROR_MSG_SIZE, "Failed to allocate Swresample context");
    Raise(EXN_FAILURE, error_msg);
  }


  swresample_set_context(swr,
			 in_channel_layout, in_sample_fmt, in_sample_rate,
			 out_channel_layout, out_sample_fmt, out_sample_rate);

  if(in_vector_kind != Frm) {
    swr->in_data = (uint8_t**)calloc(swr->in_nb_channels, sizeof(uint8_t*));
    swr->in_vector_is_array = av_sample_fmt_is_planar(swr->in_sample_fmt);

    swr->in_bytes_per_samples = av_get_bytes_per_sample(in_sample_fmt);

    if(swr->in_vector_is_array) {
      swr->in_bytes_per_samples = swr->in_bytes_per_samples * swr->in_nb_channels;
    }
  }
  
  if(av_sample_fmt_is_planar(swr->out_sample_fmt)
     && out_vector_kind != Frm) {
    swr->out_data = (uint8_t**)calloc(swr->out_nb_channels, sizeof(uint8_t*));
    swr->out_vector = caml_alloc(swr->out_nb_channels, 0);
  }
  else {
    swr->out_vector = Val_unit;
  }
  caml_register_generational_global_root(&swr->out_vector);

  switch(in_vector_kind) {
  case Frm :
    swr->get_in_samples = get_in_samples_frame;
    swr->set_in_data = set_in_data_frame;
    break;
  case Str :
    swr->get_in_samples = get_in_samples_string;
    swr->set_in_data = set_in_data_string;
    break;
  case P_Str :
    swr->get_in_samples = get_in_samples_planar_string;
    swr->set_in_data = set_in_data_planar_string;
    break;
  case Fa :
    swr->get_in_samples = get_in_samples_float_array;
    swr->set_in_data = set_in_data_float_array;
    break;
  case P_Fa :
    swr->get_in_samples = get_in_samples_planar_float_array;
    swr->set_in_data = set_in_data_planar_float_array;
    break;
  case Ba :
    swr->get_in_samples = get_in_samples_ba;
    swr->set_in_data = set_in_data_ba;
    break;
  case P_Ba :
    swr->get_in_samples = get_in_samples_planar_ba;
    swr->set_in_data = set_in_data_planar_ba;
  }

  switch(out_vector_kind) {
  case Frm :
    swr->alloc_out_vector = alloc_out_frame;
    swr->convert = convert_to_frame;
    break;
  case Str :
    swr->alloc_out_vector = alloc_out_string;
    swr->convert = convert_to_string;
    break;
  case P_Str :
    swr->alloc_out_vector = alloc_out_planar_string;
    swr->convert = convert_to_planar_string;
    break;
  case Fa :
    swr->alloc_out_vector = alloc_out_float_array;
    swr->convert = convert_to_float_array;
    break;
  case P_Fa :
    swr->alloc_out_vector = alloc_out_planar_float_array;
    swr->convert = convert_to_planar_float_array;
    break;
  case Ba :
    swr->alloc_out_vector = alloc_out_ba;
    swr->convert = convert_to_ba;
    break;
  case P_Ba :
    swr->alloc_out_vector = alloc_out_planar_ba;
    swr->convert = convert_to_planar_ba;
  }
  return swr;
}

void swresample_free(swr_t *swr)
{
  if(swr->context) swr_free(&swr->context);

  if(swr->in_data && swr->set_in_data != set_in_data_frame) free(swr->in_data);
  if(swr->out_data) free(swr->out_data);
  if(swr->out_vector) caml_remove_generational_global_root(&swr->out_vector);

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

CAMLprim value ocaml_swresample_create(value _in_vector_kind, value _in_channel_layout, value _in_sample_fmt, value _in_sample_rate,
				       value _out_vector_kind, value _out_channel_layout, value _out_sample_fmt, value _out_sample_rate)
{
  CAMLparam4(_in_channel_layout, _in_sample_fmt, _out_channel_layout, _out_sample_fmt);
  CAMLlocal1(ans);

  vector_kind in_vector_kind = Int_val(_in_vector_kind);
  int64_t in_channel_layout = ChannelLayout_val(_in_channel_layout);
  enum AVSampleFormat in_sample_fmt = SampleFormat_val(_in_sample_fmt);
  int in_sample_rate = Int_val(_in_sample_rate);
  vector_kind out_vector_kind = Int_val(_out_vector_kind);
  int64_t out_channel_layout = ChannelLayout_val(_out_channel_layout);
  enum AVSampleFormat out_sample_fmt = SampleFormat_val(_out_sample_fmt);
  int out_sample_rate = Int_val(_out_sample_rate);

  swr_t * swr = swresample_create(in_vector_kind, in_channel_layout, in_sample_fmt, in_sample_rate,
			  out_vector_kind, out_channel_layout, out_sample_fmt, out_sample_rate);

  ans = caml_alloc_custom(&swr_ops, sizeof(swr_t*), 0, 1);
  Swr_val(ans) = swr;

  CAMLreturn(ans);
}

CAMLprim value ocaml_swresample_create_byte(value *argv, int argn)
{
  return ocaml_swresample_create( argv[0], argv[1], argv[2], argv[3], argv[4], argv[5], argv[6], argv[7]);
}

