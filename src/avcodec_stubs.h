#ifndef _AVCODEC_STUBS_H_               
#define _AVCODEC_STUBS_H_

#include <caml/mlvalues.h>
#include <libavcodec/avcodec.h>

/***** Codec parameters *****/

#define CodecParameters_val(v) (*(struct AVCodecParameters**)Data_custom_val(v))

void value_of_codec_parameters_copy(AVCodecParameters *src, value * pvalue);


/**** Audio codec ID ****/

enum AVCodecID AudioCodecID_val(value v);
//long AudioCodecID_val(value v);

value Val_AudioCodecID(enum AVCodecID id);


/**** Video codec ID ****/

enum AVCodecID VideoCodecID_val(value v);
//long VideoCodecID_val(value v);

value Val_VideoCodecID(enum AVCodecID id);


/**** Subtitle codec ID ****/

enum AVCodecID SubtitleCodecID_val(value v);
//long SubtitleCodecID_val(value v);

value Val_SubtitleCodecID(enum AVCodecID id);

#endif // _AVCODEC_STUBS_H_ 
