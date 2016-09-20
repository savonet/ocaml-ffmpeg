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

#endif // _AVUTIL_STUBS_H_ 
