#ifndef _AVUTIL_STUBS_H_               
#define _AVUTIL_STUBS_H_

#include <caml/mlvalues.h>

#include <libavutil/samplefmt.h>
#include <libavutil/channel_layout.h>
#include <libavutil/frame.h>

#define EXN_FAILURE "ffmpeg_exn_failure"

#define Raise(exn, msg) caml_raise_with_string(*caml_named_value(exn), (msg))

/**** Pixel format ****/

int PixelFormat_val(value);


/**** Time format ****/

#define Time_format_val(v) (Int_val(v))

int64_t second_fractions_of_time_format(int time_format);


/**** Channel layout ****/

uint64_t ChannelLayout_val(value v);

value Val_channelLayout(uint64_t cl);


/**** Sample format ****/

#define Sample_format_val(v) (Int_val(v))

enum AVSampleFormat SampleFormat_val(value v);

enum AVSampleFormat AVSampleFormat_of_Sample_format(int i);

value Val_sampleFormat(enum AVSampleFormat sf);

enum caml_ba_kind bigarray_kind_of_Sample_format(int sf);

enum caml_ba_kind bigarray_kind_of_Sample_format_val(value val);

enum caml_ba_kind bigarray_kind_of_AVSampleFormat(enum AVSampleFormat sf);


/***** AVFrame *****/

#define Frame_val(v) (*(struct AVFrame**)Data_custom_val(v))

void value_of_frame(AVFrame *frame, value * pvalue);

#endif // _AVUTIL_STUBS_H_ 
