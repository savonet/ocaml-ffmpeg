#ifndef _AVUTIL_STUBS_H_               
#define _AVUTIL_STUBS_H_

#include <stdio.h>

#include <caml/mlvalues.h>

#include <libavutil/samplefmt.h>
#ifdef HAS_CHANNEL_LAYOUT
#include <libavutil/channel_layout.h>
#endif
#ifdef HAS_FRAME
#include <libavutil/frame.h>
#else
#include <libavformat/avformat.h>
//typedef void AVFrame;
#endif
#include <libavcodec/avcodec.h>

#define ERROR_MSG_SIZE 256
#define EXN_FAILURE "ffmpeg_exn_failure"

#define Log(...) snprintf(ocaml_av_error_msg, ERROR_MSG_SIZE, __VA_ARGS__)

#define Fail(...) {                             \
    Log(__VA_ARGS__);                           \
    return NULL;                                \
  }

#define Raise(exn, ...) {                                               \
    snprintf(ocaml_av_exn_msg, ERROR_MSG_SIZE, __VA_ARGS__);            \
    caml_raise_with_string(*caml_named_value(exn), (ocaml_av_exn_msg)); \
  }

extern char ocaml_av_error_msg[];
extern char ocaml_av_exn_msg[];

/**** Pixel format ****/

int PixelFormat_val(value);

value Val_pixelFormat(enum AVPixelFormat pf);


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
enum caml_ba_kind bigarray_kind_of_AVSampleFormat(enum AVSampleFormat sf);


/**** Media Type ****/

enum AVMediaType MediaType_val(value v);


/***** AVFrame *****/

#define Frame_val(v) (*(struct AVFrame**)Data_custom_val(v))

void value_of_frame(AVFrame *frame, value * pvalue);


/***** AVSubtitle *****/

#define SUBTITLE_TIME_BASE 100
#define SUBTITLE_TIME_BASE_Q (AVRational){1, SUBTITLE_TIME_BASE}

#define Subtitle_val(v) (*(struct AVSubtitle**)Data_custom_val(v))

void value_of_subtitle(AVSubtitle *subtitle, value * pvalue);

int subtitle_header_default(AVCodecContext *avctx);

#endif // _AVUTIL_STUBS_H_ 
