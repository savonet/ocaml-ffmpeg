#ifndef _AVUTIL_STUBS_H_               
#define _AVUTIL_STUBS_H_

#include <caml/mlvalues.h>

#include <libavutil/samplefmt.h>
#include <libavutil/channel_layout.h>

/**** Pixel format ****/

int PixelFormat_val(value);


/**** Channel layout ****/

uint64_t ChannelLayout_val(value v);

value Val_channelLayout(uint64_t cl);


/**** Sample format ****/

enum AVSampleFormat SampleFormat_val(value v);

value Val_sampleFormat(enum AVSampleFormat sf);

/**** Sample format ****/

enum AVSampleFormat SampleFormat_val(value v);

value Val_sampleFormat(enum AVSampleFormat sf);

/***** AVFrame *****/

#define Frame_val(v) (*(struct AVFrame**)Data_custom_val(v))

value value_of_frame(AVFrame *frame);

#endif // _AVUTIL_STUBS_H_ 
