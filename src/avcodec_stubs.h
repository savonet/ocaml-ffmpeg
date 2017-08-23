#ifndef _AVCODEC_STUBS_H_               
#define _AVCODEC_STUBS_H_

#include <caml/mlvalues.h>
#include <libavcodec/avcodec.h>

/**** Audio codec ID ****/

enum AVCodecID AudioCodecId_val(value v);

value Val_audioCodecId(enum AVCodecID id);

/**** Video codec ID ****/

enum AVCodecID VideoCodecId_val(value v);

value Val_videoCodecId(enum AVCodecID id);

/**** Subtitle codec ID ****/

enum AVCodecID SubtitleCodecId_val(value v);

value Val_subtitleCodecId(enum AVCodecID id);

#endif // _AVCODEC_STUBS_H_ 
