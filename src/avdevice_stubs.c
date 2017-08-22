#include <caml/mlvalues.h>
#include <caml/memory.h>
#include <caml/alloc.h>
#include <caml/fail.h>
#include <caml/callback.h>
#include <caml/custom.h>
#include <caml/threads.h>

#include <libavdevice/avdevice.h>

#include "avutil_stubs.h"
#include "av_stubs.h"

static value get_input_devices(AVInputFormat * (*input_device_next)(AVInputFormat *))
{
  CAMLparam0();
  CAMLlocal2(v, list);
  AVInputFormat *fmt = NULL;
  int len = 0;

  avdevice_register_all();

  while(NULL != (fmt = input_device_next (fmt))) len++;
  
  list = caml_alloc_tuple(len);

  int i = 0;
  fmt = NULL;

  while(NULL != (fmt = input_device_next (fmt))) {

    value_of_inputFormat(fmt, &v);
    Store_field(list, i, v);
    i++;
  }

  CAMLreturn(list);
}
  
CAMLprim value ocaml_avdevice_get_input_audio_devices(value unit)
{
  CAMLparam0();
  CAMLreturn(get_input_devices(av_input_audio_device_next));
}
  
CAMLprim value ocaml_avdevice_get_input_video_devices(value unit)
{
  CAMLparam0();
  CAMLreturn(get_input_devices(av_input_video_device_next));
}

static value get_output_devices(AVOutputFormat * (*output_device_next)(AVOutputFormat *))
{
  CAMLparam0();
  CAMLlocal2(v, list);
  AVOutputFormat *fmt = NULL;
  int len = 0;

  avdevice_register_all();

  while(NULL != (fmt = output_device_next (fmt))) len++;
  
  list = caml_alloc_tuple(len);

  int i = 0;
  fmt = NULL;

  while(NULL != (fmt = output_device_next (fmt))) {

    value_of_outputFormat(fmt, &v);
    Store_field(list, i, v);
    i++;
  }

  CAMLreturn(list);
}
  
CAMLprim value ocaml_avdevice_get_output_audio_devices(value unit)
{
  CAMLparam0();
  CAMLreturn(get_output_devices(av_output_audio_device_next));
}
  
CAMLprim value ocaml_avdevice_get_output_video_devices(value unit)
{
  CAMLparam0();
  CAMLreturn(get_output_devices(av_output_video_device_next));
}