#include <caml/mlvalues.h>
#include <caml/memory.h>
#include <caml/alloc.h>
#include <caml/fail.h>
#include <caml/callback.h>
#include <caml/custom.h>
#include <caml/bigarray.h>
#include <caml/threads.h>

#include <libavutil/pixfmt.h>
#include <libavutil/pixdesc.h>
#include "libavutil/avstring.h"

#include "avutil_stubs.h"

char ocaml_av_error_msg[ERROR_MSG_SIZE + 1];
char ocaml_av_exn_msg[ERROR_MSG_SIZE + 1];

static const enum AVPixelFormat PIXEL_FORMATS[] = {
  AV_PIX_FMT_YUV420P,
  AV_PIX_FMT_YUYV422,
  AV_PIX_FMT_RGB24,
  AV_PIX_FMT_BGR24,
  AV_PIX_FMT_YUV422P,
  AV_PIX_FMT_YUV444P,
  AV_PIX_FMT_YUV410P,
  AV_PIX_FMT_YUV411P,
  AV_PIX_FMT_YUVJ422P,
  AV_PIX_FMT_YUVJ444P,
  AV_PIX_FMT_RGBA,
  AV_PIX_FMT_BGRA
};
#define PIXEL_FORMATS_LEN (sizeof(PIXEL_FORMATS) / sizeof(enum AVPixelFormat))

enum AVPixelFormat PixelFormat_val(value v)
{
  return PIXEL_FORMATS[Int_val(v)];
}

value Val_pixelFormat(enum AVPixelFormat pf)
{
  int i;
  for (i = 0; i < PIXEL_FORMATS_LEN; i++) {
    if (pf == PIXEL_FORMATS[i])
      return Val_int(i);
  }
  Raise(EXN_FAILURE, "Invalid pixel format : %d", pf);
  return Val_int(0);
}

CAMLprim value ocaml_avutil_bits_per_pixel(value pixel)
{
  CAMLparam1(pixel);
  enum AVPixelFormat p = PixelFormat_val(pixel);
  int ans;

  ans = av_get_bits_per_pixel(av_pix_fmt_desc_get(p));

  CAMLreturn(Val_int(ans));
}


/**** Time format ****/
static const int64_t SECOND_FRACTIONS[] = {
  1, 1000, 1000000, 1000000000
};
#define SECOND_FRACTIONS_LEN (sizeof(SECOND_FRACTIONS) / sizeof(int64_t))

int64_t second_fractions_of_time_format(int time_format)
{
  return SECOND_FRACTIONS[time_format];
}

CAMLprim value ocaml_avutil_time_base()
{
  CAMLparam0();
  CAMLreturn(caml_copy_int64(AV_TIME_BASE));
}

/**** Channel layout ****/

#ifdef HAS_CHANNEL_LAYOUT
static const uint64_t CHANNEL_LAYOUTS[] = {
  AV_CH_LAYOUT_MONO,
  AV_CH_LAYOUT_STEREO,
  AV_CH_LAYOUT_2POINT1,
  AV_CH_LAYOUT_2_1,
  AV_CH_LAYOUT_SURROUND,
  AV_CH_LAYOUT_3POINT1,
  AV_CH_LAYOUT_4POINT0,
  AV_CH_LAYOUT_4POINT1,
  AV_CH_LAYOUT_2_2,
  AV_CH_LAYOUT_QUAD,
  AV_CH_LAYOUT_5POINT0,
  AV_CH_LAYOUT_5POINT1,
  AV_CH_LAYOUT_5POINT0_BACK,
  AV_CH_LAYOUT_5POINT1_BACK,
  AV_CH_LAYOUT_6POINT0,
  AV_CH_LAYOUT_6POINT0_FRONT,
  AV_CH_LAYOUT_HEXAGONAL,
  AV_CH_LAYOUT_6POINT1,
  AV_CH_LAYOUT_6POINT1_BACK,
  AV_CH_LAYOUT_6POINT1_FRONT,
  AV_CH_LAYOUT_7POINT0,
  AV_CH_LAYOUT_7POINT0_FRONT,
  AV_CH_LAYOUT_7POINT1,
  AV_CH_LAYOUT_7POINT1_WIDE,
  AV_CH_LAYOUT_7POINT1_WIDE_BACK,
  AV_CH_LAYOUT_OCTAGONAL,
  //  AV_CH_LAYOUT_HEXADECAGONAL,
  AV_CH_LAYOUT_STEREO_DOWNMIX
};
#define CHANNEL_LAYOUTS_LEN (sizeof(CHANNEL_LAYOUTS) / sizeof(uint64_t))
#endif

uint64_t ChannelLayout_val(value v)
{
#ifndef HAS_CHANNEL_LAYOUT
  caml_failwith("Not implemented.");
#else
  return CHANNEL_LAYOUTS[Int_val(v)];
#endif
}

value Val_channelLayout(uint64_t cl)
{
#ifndef HAS_CHANNEL_LAYOUT
  caml_failwith("Not implemented.");
#else
  int i;
  for (i = 0; i < CHANNEL_LAYOUTS_LEN; i++) {
    if (cl == CHANNEL_LAYOUTS[i])
      return Val_int(i);
  }
  Raise(EXN_FAILURE, "Invalid channel layout : %u", (unsigned int)cl);
  return Val_int(0);
#endif
}


/**** Sample format ****/

static const enum AVSampleFormat SAMPLE_FORMATS[] = {
  AV_SAMPLE_FMT_NONE,
  AV_SAMPLE_FMT_U8,
  AV_SAMPLE_FMT_S16,
  AV_SAMPLE_FMT_S32,
  AV_SAMPLE_FMT_FLT,
  AV_SAMPLE_FMT_DBL,
  AV_SAMPLE_FMT_U8P,
  AV_SAMPLE_FMT_S16P,
  AV_SAMPLE_FMT_S32P,
  AV_SAMPLE_FMT_FLTP,
  AV_SAMPLE_FMT_DBLP
};
#define SAMPLE_FORMATS_LEN (sizeof(SAMPLE_FORMATS) / sizeof(enum AVSampleFormat))

enum AVSampleFormat SampleFormat_val(value v)
{
  return SAMPLE_FORMATS[Int_val(v)];
}

enum AVSampleFormat AVSampleFormat_of_Sample_format(int i)
{
  return SAMPLE_FORMATS[i];
}

value Val_sampleFormat(enum AVSampleFormat sf)
{
  int i;
  for (i = 0; i < SAMPLE_FORMATS_LEN; i++) {
    if (sf == SAMPLE_FORMATS[i])
      return Val_int(i);
  }
  Raise(EXN_FAILURE, "Invalid sample format : %d", sf);
  return Val_int(0);
}

static const enum caml_ba_kind BIGARRAY_KINDS[SAMPLE_FORMATS_LEN] = {
  CAML_BA_KIND_MASK,
  CAML_BA_UINT8,
  CAML_BA_SINT16,
  CAML_BA_INT32,
  CAML_BA_FLOAT32,
  CAML_BA_FLOAT64,
  CAML_BA_UINT8,
  CAML_BA_SINT16,
  CAML_BA_INT32,
  CAML_BA_FLOAT32,
  CAML_BA_FLOAT64
};

enum caml_ba_kind bigarray_kind_of_AVSampleFormat(enum AVSampleFormat sf)
{
  int i;
  for (i = 0; i < SAMPLE_FORMATS_LEN; i++)
    {
      if (sf == SAMPLE_FORMATS[i])
        return BIGARRAY_KINDS[i];
    }
  return CAML_BA_KIND_MASK;
}

CAMLprim value ocaml_avutil_get_sample_fmt_name(value _sample_fmt)
{
  CAMLparam1(_sample_fmt);
  CAMLlocal1(ans);

  const char *name = av_get_sample_fmt_name(SampleFormat_val(_sample_fmt));
  ans = caml_copy_string(name);

  CAMLreturn(ans);
}


/**** Media Type ****/

static const enum AVMediaType MEDIA_TYPES[] = {
  AVMEDIA_TYPE_AUDIO,
  AVMEDIA_TYPE_VIDEO,
  AVMEDIA_TYPE_SUBTITLE,
  AVMEDIA_TYPE_DATA,
  AVMEDIA_TYPE_ATTACHMENT,
  AVMEDIA_TYPE_UNKNOWN
};
#define MEDIA_TYPES_LEN (sizeof(MEDIA_TYPES) / sizeof(enum AVMediaType))

enum AVMediaType MediaType_val(value v)
{
  return MEDIA_TYPES[Int_val(v)];
}


/***** AVFrame *****/

static void finalize_frame(value v)
{
#ifdef HAS_FRAME
  struct AVFrame *frame = Frame_val(v);
  av_frame_free(&frame);
#endif
}

static struct custom_operations frame_ops =
  {
    "ocaml_avframe",
    finalize_frame,
    custom_compare_default,
    custom_hash_default,
    custom_serialize_default,
    custom_deserialize_default
  };

void value_of_frame(AVFrame *frame, value * pvalue)
{
  if( ! frame) Raise(EXN_FAILURE, "Empty frame");

  *pvalue = caml_alloc_custom(&frame_ops, sizeof(AVFrame*), 0, 1);
  Frame_val((*pvalue)) = frame;
}

CAMLprim value ocaml_avutil_video_create_frame(value _w, value _h, value _format)
{
  CAMLparam1(_format);
  CAMLlocal1(ans);

#ifndef HAS_FRAME
  caml_failwith("Not implemented.");
#else
  AVFrame *frame = av_frame_alloc();
  if( ! frame) Raise(EXN_FAILURE, "Failed to alloc video frame");

  frame->format = PixelFormat_val(_format);
  frame->width  = Int_val(_w);
  frame->height = Int_val(_h);

  int ret = av_frame_get_buffer(frame, 32);
  if(ret < 0) Raise(EXN_FAILURE, "Failed to alloc video frame buffer");

  value_of_frame(frame, &ans);
#endif
  CAMLreturn(ans);
}

CAMLprim value ocaml_avutil_video_frame_get(value _frame, value _line, value _x, value _y)
{
  CAMLparam1(_frame);
  CAMLlocal1(ans);
#ifndef HAS_FRAME
  caml_failwith("Not implemented.");
#else
  AVFrame *frame = Frame_val(_frame);
  int line = Int_val(_line);
  int x = Int_val(_x);
  int y = Int_val(_y);

  ans = Val_int(frame->data[line][y * frame->linesize[line] + x]);
#endif
  CAMLreturn(ans);
}

CAMLprim value ocaml_avutil_video_frame_set(value _frame, value _line, value _x, value _y, value _v)
{
  CAMLparam1(_frame);
#ifndef HAS_FRAME
  caml_failwith("Not implemented.");
#else
  AVFrame *frame = Frame_val(_frame);
  int line = Int_val(_line);
  int x = Int_val(_x);
  int y = Int_val(_y);

  frame->data[line][y * frame->linesize[line] + x] = Int_val(_v);
#endif
  CAMLreturn(Val_unit);
}

/***** AVSubtitle *****/

static void finalize_subtitle(value v)
{
  struct AVSubtitle *subtitle = Subtitle_val(v);

  avsubtitle_free(subtitle);
  free(subtitle);
}

static struct custom_operations subtitle_ops =
  {
    "ocaml_avsubtitle",
    finalize_subtitle,
    custom_compare_default,
    custom_hash_default,
    custom_serialize_default,
    custom_deserialize_default
  };

void value_of_subtitle(AVSubtitle *subtitle, value * pvalue)
{
  if( ! subtitle) Raise(EXN_FAILURE, "Empty subtitle");

  *pvalue = caml_alloc_custom(&subtitle_ops, sizeof(AVSubtitle*), 0, 1);
  Subtitle_val((*pvalue)) = subtitle;
}


CAMLprim value ocaml_avutil_subtitle_time_base()
{
  CAMLparam0();
  CAMLreturn(caml_copy_int64(SUBTITLE_TIME_BASE));
}
/*
int subtitle_header(AVCodecContext *codec_context,
                    const char *font, int font_size,
                    int color, int back_color,
                    int bold, int italic, int underline,
                    int border_style, int alignment)
{
  char * sh = av_asprintf("[Script Info]\r\n; Script generated by ocaml-ffmpeg\r\n"
                          "ScriptType: v4.00+\r\nPlayResX: 384\r\nPlayResY: 288\r\n\r\n"
                          "[V4+ Styles]\r\nFormat: Name, Fontname, Fontsize, "
                          "PrimaryColour, SecondaryColour, OutlineColour, BackColour, "
                          "Bold, Italic, Underline, StrikeOut, ScaleX, ScaleY, "
                          "Spacing, Angle, BorderStyle, Outline, Shadow, "
                          "Alignment, MarginL, MarginR, MarginV, Encoding\r\n"
                          "Style: Default,%s,%d,&H%x,&H%x,&H%x,&H%x,%d,%d,%d,0,"
                          "100,100,0,0,%d,1,0,%d,10,10,10,0\r\n\r\n"
                          "[Events]\r\n"
                          "Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\r\n", font, font_size, color, color, back_color, back_color, -bold, -italic, -underline, border_style, alignment);
  if ( ! sh)
    return AVERROR(ENOMEM);

  codec_context->subtitle_header_size = strlen(sh);
  codec_context->subtitle_header = (uint8_t*)sh;

  return 0;
}
*/
int subtitle_header_default(AVCodecContext *codec_context)
{
  //  return subtitle_header(codec_context, "Arial", 16, 0xffffff, 0, 0, 0, 0, 1, 2);
  return 0;
}
/*
char *get_dialog(int readorder, int layer, const char *style,
                 const char *speaker, const char *text)
{
  char * dialog = av_asprintf("%d,%d,%s,%s,0,0,0,,%s",
                              readorder, layer, style ? style : "Default",
                              speaker ? speaker : "", text);
  return dialog;
}
*/
CAMLprim value ocaml_avutil_subtitle_create_frame(value _start_time, value _end_time, value _lines)
{
  CAMLparam3(_start_time, _end_time, _lines);
  CAMLlocal1(ans);
  int64_t start_time = Int64_val(_start_time);
  int64_t end_time = Int64_val(_end_time);
  int nb_lines = Wosize_val(_lines);

  AVSubtitle * subtitle = (AVSubtitle*)calloc(1, sizeof(AVSubtitle));
  if( ! subtitle) Raise(EXN_FAILURE, "Failed to allocate subtitle frame");

  value_of_subtitle(subtitle, &ans);

  //  subtitle->start_display_time = (uint32_t)start_time;
  subtitle->end_display_time = (uint32_t)end_time;
  subtitle->pts = start_time;

  subtitle->rects = (AVSubtitleRect **)av_malloc_array(nb_lines, sizeof(AVSubtitleRect *));
  if( ! subtitle->rects) Raise(EXN_FAILURE, "Failed to allocate subtitle frame");

  subtitle->num_rects = nb_lines;
  
  int i;
  for(i = 0; i < nb_lines; i++) {
    const char * text = String_val(Field(_lines, i));
    subtitle->rects[i] = (AVSubtitleRect *)av_mallocz(sizeof(AVSubtitleRect));
    if( ! subtitle->rects[i]) Raise(EXN_FAILURE, "Failed to allocate subtitle frame");

    subtitle->rects[i]->type = SUBTITLE_TEXT;
    subtitle->rects[i]->text = strdup(text);
    if( ! subtitle->rects[i]->text) Raise(EXN_FAILURE, "Failed to allocate subtitle frame");

    //    subtitle->rects[i]->type = SUBTITLE_ASS;
    //    subtitle->rects[i]->ass = get_dialog(i + 1, 0, NULL, NULL, text);
    //    if( ! subtitle->rects[i]->ass) Raise(EXN_FAILURE, "Failed to allocate subtitle frame");
  }

  CAMLreturn(ans);
}

CAMLprim value ocaml_avutil_subtitle_to_lines(value _subtitle)
{
  CAMLparam1(_subtitle);
  CAMLlocal2(ans, lines);
  struct AVSubtitle *subtitle = Subtitle_val(_subtitle);
  unsigned i, num_rects = subtitle->num_rects;

  lines = caml_alloc_tuple(num_rects);

  for(i = 0; i < num_rects; i++) {
    char * line = subtitle->rects[i]->text ? subtitle->rects[i]->text : subtitle->rects[i]->ass;
    Store_field(lines, i, caml_copy_string(line));
  }

  ans = caml_alloc_tuple(3);
  Store_field(ans, 0, caml_copy_int64((int64_t)subtitle->start_display_time));
  Store_field(ans, 1, caml_copy_int64((int64_t)subtitle->end_display_time));
  Store_field(ans, 2, lines);

  CAMLreturn(ans);
}


