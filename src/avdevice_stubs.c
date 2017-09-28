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
  CAMLlocal2(v, ans);
  AVInputFormat *fmt = NULL;
  int len = 0;

  avdevice_register_all();

  while((fmt = input_device_next(fmt))) len++;
  
  ans = caml_alloc_tuple(len);

  int i = 0;
  fmt = NULL;

  while((fmt = input_device_next(fmt))) {

    value_of_inputFormat(fmt, &v);
    Store_field(ans, i, v);
    i++;
  }

  CAMLreturn(ans);
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
  CAMLlocal2(v, ans);
  AVOutputFormat *fmt = NULL;
  int len = 0;

  avdevice_register_all();

  while((fmt = output_device_next (fmt))) len++;
  
  ans = caml_alloc_tuple(len);

  int i = 0;
  fmt = NULL;

  while((fmt = output_device_next (fmt))) {

    value_of_outputFormat(fmt, &v);
    Store_field(ans, i, v);
    i++;
  }

  CAMLreturn(ans);
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

static const enum AVAppToDevMessageType APP_TO_DEV_MESSAGE_TYPES[] = {
  AV_APP_TO_DEV_NONE,
  AV_APP_TO_DEV_PAUSE,
  AV_APP_TO_DEV_PLAY,
  AV_APP_TO_DEV_TOGGLE_PAUSE,
  AV_APP_TO_DEV_MUTE,
  AV_APP_TO_DEV_UNMUTE,
  AV_APP_TO_DEV_TOGGLE_MUTE,
  AV_APP_TO_DEV_GET_VOLUME,
  AV_APP_TO_DEV_GET_MUTE
};

static const enum AVAppToDevMessageType APP_TO_DEV_MESSAGE_WITH_DATA_TYPES[] = {
  AV_APP_TO_DEV_WINDOW_SIZE,
  AV_APP_TO_DEV_WINDOW_REPAINT,
  AV_APP_TO_DEV_SET_VOLUME
};

CAMLprim value ocaml_avdevice_app_to_dev_control_message(value _message, value _av)
{
  CAMLparam2(_message, _av);
  enum AVAppToDevMessageType message_type;
  void * data = NULL;
  size_t data_size = 0;
  double dbl;
  AVDeviceRect rect;

  if (Is_block(_message)) {
    message_type = APP_TO_DEV_MESSAGE_WITH_DATA_TYPES[Tag_val(_message)];

    if(message_type == AV_APP_TO_DEV_SET_VOLUME) {
      dbl = Double_val(Field(_message, 0));
      data = &dbl;
      data_size = sizeof(dbl);
    }
    else {
      rect.x = Int_val(Field(_message, 0));
      rect.y = Int_val(Field(_message, 1));
      rect.width = Int_val(Field(_message, 2));
      rect.height = Int_val(Field(_message, 3));

      if(message_type == AV_APP_TO_DEV_WINDOW_SIZE || rect.width > 0) {
        data = &rect;
        data_size = sizeof(rect);
      }
    }
  }
  else {
    message_type = APP_TO_DEV_MESSAGE_TYPES[Int_val(_message)];
  }

  int ret = avdevice_app_to_dev_control_message(ocaml_av_get_format_context(&_av), message_type, data, data_size);
  if(ret == AVERROR(ENOSYS)) Raise(EXN_FAILURE, "App to device control message failed : device doesn't implement handler of the message");
  if(ret < 0) Raise(EXN_FAILURE, "App to device control message failed : %s", av_err2str(ret));

  CAMLreturn(Val_unit);
}


#define NONE_TAG 0
#define CREATE_WINDOW_BUFFER_TAG 0
#define PREPARE_WINDOW_BUFFER_TAG 1
#define DISPLAY_WINDOW_BUFFER_TAG 2
#define DESTROY_WINDOW_BUFFER_TAG 3
#define BUFFER_OVERFLOW_TAG 4
#define BUFFER_UNDERFLOW_TAG 5
#define BUFFER_READABLE_TAG 1
#define BUFFER_WRITABLE_TAG 2
#define MUTE_STATE_CHANGED_TAG 3
#define VOLUME_LEVEL_CHANGED_TAG 4

int control_message_callback(struct AVFormatContext *ctx, int type, void *data, size_t data_size)
{
  CAMLparam0();
  CAMLlocal3(msg, opt, res);
  enum AVDevToAppMessageType message_type = (enum AVDevToAppMessageType)type;
  int ret = 0;

  if(message_type == AV_DEV_TO_APP_NONE) {
    msg = Val_int(NONE_TAG);
  }
  else if(message_type == AV_DEV_TO_APP_CREATE_WINDOW_BUFFER) {
    if(data) {
      AVDeviceRect * rect = (AVDeviceRect *)data;
      opt = caml_alloc_tuple(4);
      Store_field(opt, 0, Val_int(rect->x));
      Store_field(opt, 1, Val_int(rect->y));
      Store_field(opt, 2, Val_int(rect->width));
      Store_field(opt, 3, Val_int(rect->height));
    }
    else {
      opt = Val_int(0);
    }

    msg = caml_alloc(1, CREATE_WINDOW_BUFFER_TAG);
    Store_field(msg, 0, opt);
  }
  else if(message_type == AV_DEV_TO_APP_PREPARE_WINDOW_BUFFER) {
    msg = Val_int(PREPARE_WINDOW_BUFFER_TAG);
  }
  else if(message_type == AV_DEV_TO_APP_DISPLAY_WINDOW_BUFFER) {
    msg = Val_int(DISPLAY_WINDOW_BUFFER_TAG);
  }
  else if(message_type == AV_DEV_TO_APP_DESTROY_WINDOW_BUFFER) {
    msg = Val_int(DESTROY_WINDOW_BUFFER_TAG);
  }
  else if(message_type == AV_DEV_TO_APP_BUFFER_OVERFLOW) {
    msg = Val_int(BUFFER_OVERFLOW_TAG);
  }
  else if(message_type == AV_DEV_TO_APP_BUFFER_UNDERFLOW) {
    msg = Val_int(BUFFER_UNDERFLOW_TAG);
  }
  else if(message_type == AV_DEV_TO_APP_BUFFER_READABLE || message_type == AV_DEV_TO_APP_BUFFER_WRITABLE) {

    if(data) {
      opt = caml_alloc_tuple(1);
      Store_field(opt, 0,caml_copy_int64(*((int64_t*)data)));
    }
    else {
      opt = Val_int(0);
    }
    msg = caml_alloc(1, message_type == AV_DEV_TO_APP_BUFFER_READABLE ? BUFFER_READABLE_TAG : BUFFER_WRITABLE_TAG);
    Store_field(msg, 0, opt);
  }
  else if(message_type == AV_DEV_TO_APP_MUTE_STATE_CHANGED) {
    msg = caml_alloc(1, MUTE_STATE_CHANGED_TAG);
    Store_field(msg, 0, (*((int*)data)) ? Val_true : Val_false);
  }
  else if(message_type == AV_DEV_TO_APP_VOLUME_LEVEL_CHANGED) {
    msg = caml_alloc(1, VOLUME_LEVEL_CHANGED_TAG);
    Store_field(msg, 0, caml_copy_double(*((double*)data)));
  }
  res = caml_callback_exn(*ocaml_av_get_control_message_callback(ctx), msg);

  if(Is_exception_result(res)) {
    res = Extract_exception(res);
    ret = AVERROR_UNKNOWN;
  }
  CAMLreturn(ret);
}

CAMLprim value ocaml_avdevice_set_control_message_callback(value _control_message_callback, value _av)
{
  CAMLparam2(_control_message_callback, _av);

  ocaml_av_set_control_message_callback(&_av, control_message_callback, &_control_message_callback);

  CAMLreturn(Val_unit);
}
