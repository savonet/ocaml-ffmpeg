#ifndef _AVCODEC_STUBS_H_               
#define _AVCODEC_STUBS_H_

#include <caml/mlvalues.h>
#include <libavcodec/avcodec.h>

/***** Codec parameters *****/

#define CodecParameters_val(v) (*(struct AVCodecParameters**)Data_custom_val(v))

void value_of_codec_parameters_copy(AVCodecParameters *src, value * pvalue);


/***** Packet *****/

#define Packet_val(v) (*(struct AVPacket**)Data_custom_val(v))

value value_of_ffmpeg_packet(AVPacket *packet);

/**** Audio codec ID ****/

enum AVCodecID AudioCodecID_val(value v);

value Val_AudioCodecID(enum AVCodecID id);


/**** Video codec ID ****/

enum AVCodecID VideoCodecID_val(value v);

value Val_VideoCodecID(enum AVCodecID id);


/**** Subtitle codec ID ****/

enum AVCodecID SubtitleCodecID_val(value v);

value Val_SubtitleCodecID(enum AVCodecID id);

#endif // _AVCODEC_STUBS_H_ 
