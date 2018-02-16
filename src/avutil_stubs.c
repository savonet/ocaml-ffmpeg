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
#include "pixel_format_stubs.h"
#include "channel_layout_stubs.h"
#include "sample_format_stubs.h"

char ocaml_av_error_msg[ERROR_MSG_SIZE + 1];
char ocaml_av_exn_msg[ERROR_MSG_SIZE + 1];

CAMLprim value ocaml_avutil_bits_per_pixel(value pixel)
{
  CAMLparam1(pixel);
  enum AVPixelFormat p = PixelFormat_val(pixel);
  int ans;

  ans = av_get_bits_per_pixel(av_pix_fmt_desc_get(p));

  CAMLreturn(Val_int(ans));
}


/**** Time format ****/
int64_t second_fractions_of_time_format(value time_format)
{
  switch(time_format) {
  case PVV_second : return 1;
  case PVV_millisecond : return 1000;
  case PVV_microsecond : return 1000000;
  case PVV_nanosecond : return 1000000000;
  default : break;
  }
  return 1;
}

CAMLprim value ocaml_avutil_time_base()
{
  CAMLparam0();
  CAMLreturn(caml_copy_int64(AV_TIME_BASE));
}

/**** Channel layout ****/

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
  if(ret < 0) Raise(EXN_FAILURE, "Failed to alloc video frame buffer : %s", av_err2str(ret));

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

  if(line < 0 || line >= AV_NUM_DATA_POINTERS || ! frame->data[line]) Raise(EXN_FAILURE, "Failed to get from video frame : line (%d) out of boundaries", line);
  int linesize = frame->linesize[line];
  if(x < 0 || x >= linesize) Raise(EXN_FAILURE, "Failed to get from video frame : x (%d) out of 0-%d boundaries", x, linesize - 1);
  if(y < 0 || y >= frame->height) Raise(EXN_FAILURE, "Failed to get from video frame : y (%d) out of 0-%d boundaries", y, frame->height - 1);

  ans = Val_int(frame->data[line][y * linesize + x]);
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

  if(line < 0 || line >= AV_NUM_DATA_POINTERS || ! frame->data[line]) Raise(EXN_FAILURE, "Failed to set to video frame : line (%d) out of boundaries", line);
  int linesize = frame->linesize[line];
  if(x < 0 || x >= linesize) Raise(EXN_FAILURE, "Failed to set to video frame : x (%d) out of 0-%d boundaries", x, linesize - 1);
  if(y < 0 || y >= frame->height) Raise(EXN_FAILURE, "Failed to set to video frame : y (%d) out of 0-%d boundaries", y, frame->height - 1);

  frame->data[line][y * linesize + x] = Int_val(_v);
#endif
  CAMLreturn(Val_unit);
}

CAMLprim value ocaml_avutil_video_frame_get_linesize(value _frame, value _line)
{
  CAMLparam1(_frame);
#ifndef HAS_FRAME
  caml_failwith("Not implemented.");
#else
  AVFrame *frame = Frame_val(_frame);
  int line = Int_val(_line);

  if(line < 0 || line >= AV_NUM_DATA_POINTERS || ! frame->data[line]) Raise(EXN_FAILURE, "Failed to get linesize from video frame : line (%d) out of boundaries", line);
#endif
  CAMLreturn(Val_int(frame->linesize[line]));
}

CAMLprim intnat ocaml_avutil_video_frame_planar_get(value _frame, intnat line, intnat p)
{
  CAMLparam1(_frame);
  CAMLlocal1(ans);
#ifndef HAS_FRAME
  caml_failwith("Not implemented.");
  CAMLreturn(ans);
#else
  AVFrame *frame = Frame_val(_frame);
  /*
    if(line < 0 || line >= AV_NUM_DATA_POINTERS || ! frame->data[line]) Raise(EXN_FAILURE, "Failed to set to video frame : line (%d) out of boundaries", line);
    if(p < 0 || p >= frame->buf[line]->size) Raise(EXN_FAILURE, "Failed to set to video frame plane : p (%d) out of 0-%d boundaries", p, frame->buf[line]->size);
    ans = Val_int();
  */
  CAMLreturn((intnat)frame->data[line][p]);
#endif
}

CAMLprim value ocaml_avutil_video_frame_planar_get_byte(value _frame, value _line, value _p)
{
  return ocaml_avutil_video_frame_planar_get(_frame, Int_val(_line), Int_val(_p));
}

CAMLprim value ocaml_avutil_video_frame_planar_set(value _frame, intnat line, intnat p, intnat v)
{
  CAMLparam1(_frame);
#ifndef HAS_FRAME
  caml_failwith("Not implemented.");
#else
  AVFrame *frame = Frame_val(_frame);
  /*
    int line = Int_val(_line);
    int p = Int_val(_p);

    if(line < 0 || line >= AV_NUM_DATA_POINTERS || ! frame->data[line]) Raise(EXN_FAILURE, "Failed to set to video frame : line (%d) out of boundaries", line);
    if(p < 0 || p >= frame->buf[line]->size) Raise(EXN_FAILURE, "Failed to set to video frame plane : p (%d) out of 0-%d boundaries", p, frame->buf[line]->size);
  */
  frame->data[line][p] = v;
#endif
  CAMLreturn(Val_unit);
}

CAMLprim value ocaml_avutil_video_frame_planar_set_byte(value _frame, value _line, value _p, value _v)
{
  return ocaml_avutil_video_frame_planar_set(_frame, Int_val(_line), Int_val(_p), Int_val(_v));
}

CAMLprim value ocaml_avutil_video_copy_frame_to_bigarray_planes(value _frame)
{
  CAMLparam1(_frame);
  CAMLlocal3(ans, data, plane);
#ifndef HAS_FRAME
  caml_failwith("Not implemented.");
#else
  AVFrame *frame = Frame_val(_frame);
  int i, nb_planes;

  for(nb_planes = 0; frame->buf[nb_planes]; nb_planes++);

  ans = caml_alloc_tuple(nb_planes);

  for(i = 0; i < nb_planes; i++) {
    AVBufferRef* buffer = frame->buf[i];
    intnat out_size = buffer->size;

    data = caml_ba_alloc(CAML_BA_C_LAYOUT | CAML_BA_UINT8, 1, NULL, &out_size);
    plane = caml_alloc_tuple(2);

    Store_field(plane, 0, data);
    Store_field(plane, 1, Val_int(frame->linesize[i]));

    memcpy(Caml_ba_data_val(data), buffer->data, buffer->size);

    Store_field(ans, i, plane);
  }
#endif
  CAMLreturn(ans);
}


CAMLprim value ocaml_avutil_video_copy_bigarray_planes_to_frame(value _planes, value _frame)
{
  CAMLparam2(_planes, _frame);
  CAMLlocal2(data, plane);
#ifndef HAS_FRAME
  caml_failwith("Not implemented.");
#else
  int i, nb_planes = Wosize_val(_planes);
  AVFrame *frame = Frame_val(_frame);

  int ret = av_frame_make_writable(frame);
  if (ret < 0) Raise(EXN_FAILURE, "Failed to make frame writable : %s", av_err2str(ret));

  for(i = 0; i < nb_planes && frame->buf[i]; i++) {
    AVBufferRef* buffer = frame->buf[i];
    plane = Field(_planes, i);
    data = Field(plane, 0);
    int src_linesize = Int_val(Field(plane, 1));

    if(src_linesize != frame->linesize[i]) Raise(EXN_FAILURE, "Failed to copy planes to frame : incompatible linesize");

    size_t size = buffer->size;

    if(Caml_ba_array_val(data)->dim[0] < size) size = Caml_ba_array_val(data)->dim[0];

    memcpy(buffer->data, Caml_ba_data_val(data), size);
  }
#endif
  CAMLreturn(Val_unit);
}



CAMLprim value ocaml_avutil_video_copy_frame_to_bytes_planes(value _frame)
{
  CAMLparam1(_frame);
  CAMLlocal3(ans, data, plane);
#ifndef HAS_FRAME
  caml_failwith("Not implemented.");
#else
  AVFrame *frame = Frame_val(_frame);
  int i, nb_planes;

  for(nb_planes = 0; frame->buf[nb_planes]; nb_planes++);

  ans = caml_alloc_tuple(nb_planes);

  for(i = 0; i < nb_planes; i++) {
    AVBufferRef* buffer = frame->buf[i];

    data = caml_alloc_string(buffer->size);
    plane = caml_alloc_tuple(2);

    Store_field(plane, 0, data);
    Store_field(plane, 1, Val_int(frame->linesize[i]));

    memcpy(String_val(data), buffer->data, buffer->size);

    Store_field(ans, i, plane);
  }
#endif
  CAMLreturn(ans);
}


CAMLprim value ocaml_avutil_video_copy_bytes_planes_to_frame(value _planes, value _frame)
{
  CAMLparam2(_planes, _frame);
  CAMLlocal2(data, plane);
#ifndef HAS_FRAME
  caml_failwith("Not implemented.");
#else
  int i, nb_planes = Wosize_val(_planes);
  AVFrame *frame = Frame_val(_frame);

  int ret = av_frame_make_writable(frame);
  if (ret < 0) Raise(EXN_FAILURE, "Failed to make frame writable : %s", av_err2str(ret));

  for(i = 0; i < nb_planes && frame->buf[i]; i++) {
    AVBufferRef* buffer = frame->buf[i];
    plane = Field(_planes, i);
    data = Field(plane, 0);
    int src_linesize = Int_val(Field(plane, 1));

    if(src_linesize != frame->linesize[i]) Raise(EXN_FAILURE, "Failed to copy planes to frame : incompatible linesize");

    int data_size = caml_string_length(data);
    size_t size = buffer->size < data_size ? buffer->size : data_size;

    memcpy(buffer->data, (uint8_t*)String_val(data), size);
  }
#endif
  CAMLreturn(Val_unit);
}




CAMLprim value ocaml_avutil_video_get_frame_planes(value _frame)
{
  CAMLparam1(_frame);
  CAMLlocal3(ans, data, plane);
#ifndef HAS_FRAME
  caml_failwith("Not implemented.");
#else
  AVFrame *frame = Frame_val(_frame);
  int i, nb_planes;

  int ret = av_frame_make_writable(frame);
  if (ret < 0) Raise(EXN_FAILURE, "Failed to make frame writable : %s", av_err2str(ret));

  for(nb_planes = 0; frame->buf[nb_planes]; nb_planes++);

  ans = caml_alloc_tuple(nb_planes);

  for(i = 0; i < nb_planes; i++) {
    AVBufferRef* buffer = av_frame_get_plane_buffer(frame, i);
    if( ! buffer) Raise(EXN_FAILURE, "Failed to get frame plane buffer");

    intnat out_size = buffer->size;

    data = caml_ba_alloc(CAML_BA_C_LAYOUT | CAML_BA_UINT8, 1, buffer->data, &out_size);
    plane = caml_alloc_tuple(3);

    Store_field(plane, 0, data);
    Store_field(plane, 1, Val_int(frame->linesize[i]));
    Store_field(plane, 2, _frame);

    Store_field(ans, i, plane);
  }
#endif
  CAMLreturn(ans);
}


CAMLprim value ocaml_avutil_video_get_frame_bigarray_planes(value _frame, value _make_writable)
{
  CAMLparam1(_frame);
  CAMLlocal2(ans, plane);
#ifndef HAS_FRAME
  caml_failwith("Not implemented.");
#else
  AVFrame *frame = Frame_val(_frame);
  int i, nb_planes;

  if(Bool_val(_make_writable)) {
    int ret = av_frame_make_writable(frame);
    if (ret < 0) Raise(EXN_FAILURE, "Failed to make frame writable : %s", av_err2str(ret));
  }
  
  for(nb_planes = 0; frame->buf[nb_planes]; nb_planes++);

  ans = caml_alloc_tuple(nb_planes);

  for(i = 0; i < nb_planes; i++) {
    AVBufferRef* buffer = av_frame_get_plane_buffer(frame, i);
    if( ! buffer) Raise(EXN_FAILURE, "Failed to get frame plane buffer");

    intnat out_size = buffer->size;
    plane = caml_alloc_tuple(2);

    Store_field(plane, 0, caml_ba_alloc(CAML_BA_C_LAYOUT | CAML_BA_UINT8, 1, buffer->data, &out_size));
    Store_field(plane, 1, Val_int(frame->linesize[i]));
    Store_field(ans, i, plane);
  }
#endif
  CAMLreturn(ans);
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


