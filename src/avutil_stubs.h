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

#include "polymorphic_variant_values_stubs.h"

#define ERROR_MSG_SIZE 256
#define EXN_FAILURE "ffmpeg_exn_failure"

#define Log(...) snprintf(ocaml_av_error_msg, ERROR_MSG_SIZE, __VA_ARGS__)

#define Fail(...) {                                             \
    snprintf(ocaml_av_error_msg, ERROR_MSG_SIZE, __VA_ARGS__);  \
    return NULL;                                                \
  }

#define Raise(exn, ...) {                                               \
    snprintf(ocaml_av_exn_msg, ERROR_MSG_SIZE, __VA_ARGS__);            \
    caml_raise_with_string(*caml_named_value(exn), (ocaml_av_exn_msg)); \
  }

extern char ocaml_av_error_msg[];
extern char ocaml_av_exn_msg[];


#define List_init(list) (list) = Val_emptylist

#define List_add(list, cons, val) {             \
    (cons) = caml_alloc(2, 0);                  \
    Store_field((cons), 0, (val));              \
    Store_field((cons), 1, (list));             \
    (list) = (cons);                            \
  }


/***** Global initialisation *****/

void ocaml_ffmpeg_register_thread();

int register_lock_manager();

  
/**** AVRational ****/
#define rational_of_value(v) ((AVRational){Int_val(Field((v), 0)), Int_val(Field((v), 1))})

void value_of_rational(const AVRational * r, value * pv);


/**** Time format ****/

int64_t second_fractions_of_time_format(value time_format);


/**** Channel layout ****/

uint64_t ChannelLayout_val(value v);

value Val_ChannelLayout(uint64_t cl);


/**** Sample format ****/

#define Sample_format_val(v) (Int_val(v))

enum AVSampleFormat SampleFormat_val(value v);

enum AVSampleFormat AVSampleFormat_of_Sample_format(int i);

value Val_SampleFormat(enum AVSampleFormat sf);
enum caml_ba_kind bigarray_kind_of_AVSampleFormat(enum AVSampleFormat sf);


/**** Pixel format ****/

int PixelFormat_val(value);

value Val_PixelFormat(enum AVPixelFormat pf);


/***** AVFrame *****/

#define Frame_val(v) (*(struct AVFrame**)Data_custom_val(v))

void value_of_frame(AVFrame *frame, value * pvalue);

AVFrame * alloc_frame_value(value * pvalue);


/***** AVSubtitle *****/
#define Subtitle_val(v) (*(struct AVSubtitle**)Data_custom_val(v))

void value_of_subtitle(AVSubtitle *subtitle, value * pvalue);

AVSubtitle * alloc_subtitle_value(value * pvalue);

int subtitle_header_default(AVCodecContext *avctx);

#endif // _AVUTIL_STUBS_H_ 
