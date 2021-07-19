#include <caml/alloc.h>
#include <caml/bigarray.h>
#include <caml/callback.h>
#include <caml/custom.h>
#include <caml/fail.h>
#include <caml/memory.h>
#include <caml/mlvalues.h>
#include <caml/threads.h>

#include "avutil_stubs.h"

#include "avcodec_stubs.h"
#include "codec_capabilities_stubs.h"
#include "codec_id_stubs.h"
#include "hw_config_method_stubs.h"

#ifndef AV_PKT_FLAG_DISPOSABLE
#define AV_PKT_FLAG_DISPOSABLE 0x0010
#endif

value ocaml_avcodec_init(value unit) {
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58, 9, 100)
  avcodec_register_all();
#endif
  return Val_unit;
}

static value value_of_audio_codec_id(enum AVCodecID id) {
  value ret = Val_AudioCodecID(id);
  return ret;
}

static value value_of_video_codec_id(enum AVCodecID id) {
  value ret = Val_VideoCodecID(id);
  return ret;
}

static value value_of_subtitle_codec_id(enum AVCodecID id) {
  value ret = Val_SubtitleCodecID(id);
  return ret;
}

CAMLprim value ocaml_avcodec_flag_qscale(value unit) {
  CAMLparam0();
  CAMLreturn(Val_int(AV_CODEC_FLAG_QSCALE));
}

CAMLprim value ocaml_avcodec_get_input_buffer_padding_size() {
  CAMLparam0();
  CAMLreturn(Val_int(AV_INPUT_BUFFER_PADDING_SIZE));
}

CAMLprim value ocaml_avcodec_subtitle_codec_id_to_AVCodecID(value _codec_id) {
  CAMLparam1(_codec_id);
  CAMLreturn(Val_int(SubtitleCodecID_val(_codec_id)));
}

/***** AVCodecContext *****/

static AVCodecContext *create_AVCodecContext(AVCodecParameters *params,
                                             AVCodec *codec) {
  AVCodecContext *codec_context;
  int ret = 0;

  caml_release_runtime_system();
  codec_context = avcodec_alloc_context3(codec);

  if (!codec_context) {
    caml_acquire_runtime_system();
    caml_raise_out_of_memory();
  }

  if (params)
    ret = avcodec_parameters_to_context(codec_context, params);

  if (ret < 0) {
    avcodec_free_context(&codec_context);
    caml_acquire_runtime_system();
    ocaml_avutil_raise_error(ret);
  }

  // Open the codec
  ret = avcodec_open2(codec_context, codec, NULL);

  if (ret < 0) {
    avcodec_free_context(&codec_context);
    caml_acquire_runtime_system();
    ocaml_avutil_raise_error(ret);
  }

  caml_acquire_runtime_system();

  return codec_context;
}

/***** AVCodecParameters *****/

static void finalize_codec_parameters(value v) {
  struct AVCodecParameters *codec_parameters = CodecParameters_val(v);
  avcodec_parameters_free(&codec_parameters);
}

static struct custom_operations codec_parameters_ops = {
    "ocaml_avcodec_parameters", finalize_codec_parameters,
    custom_compare_default,     custom_hash_default,
    custom_serialize_default,   custom_deserialize_default};

void value_of_codec_parameters_copy(AVCodecParameters *src, value *pvalue) {
  if (!src)
    Fail("Failed to get codec parameters");

  caml_release_runtime_system();
  AVCodecParameters *dst = avcodec_parameters_alloc();
  caml_acquire_runtime_system();

  if (!dst)
    caml_raise_out_of_memory();

  caml_release_runtime_system();
  int ret = avcodec_parameters_copy(dst, src);
  caml_acquire_runtime_system();

  if (ret < 0)
    ocaml_avutil_raise_error(ret);

  *pvalue = caml_alloc_custom(&codec_parameters_ops,
                              sizeof(AVCodecParameters *), 0, 1);
  CodecParameters_val(*pvalue) = dst;
}

/***** AVPacket *****/

static void finalize_packet(value v) {
  struct AVPacket *packet = Packet_val(v);
  av_packet_free(&packet);
}

static struct custom_operations packet_ops = {
    "ocaml_packet",      finalize_packet,          custom_compare_default,
    custom_hash_default, custom_serialize_default, custom_deserialize_default};

value value_of_ffmpeg_packet(AVPacket *packet) {
  value ret;

  if (!packet)
    Fail("Empty packet");

  ret = caml_alloc_custom(&packet_ops, sizeof(AVPacket *), 0, 1);
  Packet_val(ret) = packet;

  return ret;
}

CAMLprim value ocaml_avcodec_packet_dup(value _packet) {
  CAMLparam1(_packet);
  CAMLlocal1(ret);

  AVPacket *packet = av_packet_alloc();

  if (!packet)
    caml_raise_out_of_memory();

  av_packet_ref(packet, Packet_val(_packet));

  ret = caml_alloc_custom(&packet_ops, sizeof(AVPacket *), 0, 1);
  Packet_val(ret) = packet;

  CAMLreturn(ret);
}

CAMLprim value ocaml_avcodec_get_flags(value _packet) {
  CAMLparam1(_packet);
  CAMLreturn(Val_int(Packet_val(_packet)->flags));
}

CAMLprim value ocaml_avcodec_get_packet_stream_index(value _packet) {
  CAMLparam1(_packet);
  CAMLreturn(Val_int(Packet_val(_packet)->stream_index));
}

CAMLprim value ocaml_avcodec_set_packet_stream_index(value _packet,
                                                     value _index) {
  CAMLparam1(_packet);
  Packet_val(_packet)->stream_index = Int_val(_index);
  CAMLreturn(Val_unit);
}

CAMLprim value ocaml_avcodec_get_packet_pts(value _packet) {
  CAMLparam1(_packet);
  CAMLlocal1(ret);
  AVPacket *packet = Packet_val(_packet);

  if (packet->pts == AV_NOPTS_VALUE)
    CAMLreturn(Val_none);

  ret = caml_alloc_tuple(1);
  Store_field(ret, 0, caml_copy_int64(packet->pts));

  CAMLreturn(ret);
}

CAMLprim value ocaml_avcodec_set_packet_pts(value _packet, value _pts) {
  CAMLparam2(_packet, _pts);
  AVPacket *packet = Packet_val(_packet);

  if (_pts == Val_none)
    packet->pts = AV_NOPTS_VALUE;
  else
    packet->pts = Int64_val(Field(_pts, 0));

  CAMLreturn(Val_unit);
}

CAMLprim value ocaml_avcodec_get_packet_duration(value _packet) {
  CAMLparam1(_packet);
  CAMLlocal1(ret);
  AVPacket *packet = Packet_val(_packet);

  if (packet->duration == 0)
    CAMLreturn(Val_none);

  ret = caml_alloc_tuple(1);
  Store_field(ret, 0, caml_copy_int64(packet->duration));

  CAMLreturn(ret);
}

CAMLprim value ocaml_avcodec_set_packet_duration(value _packet,
                                                 value _duration) {
  CAMLparam2(_packet, _duration);
  AVPacket *packet = Packet_val(_packet);

  if (_duration == Val_none)
    packet->duration = 0;
  else
    packet->duration = Int64_val(Field(_duration, 0));

  CAMLreturn(Val_unit);
}

CAMLprim value ocaml_avcodec_get_packet_position(value _packet) {
  CAMLparam1(_packet);
  CAMLlocal1(ret);
  AVPacket *packet = Packet_val(_packet);

  if (packet->pos == -1)
    CAMLreturn(Val_none);

  ret = caml_alloc_tuple(1);
  Store_field(ret, 0, caml_copy_int64(packet->pos));

  CAMLreturn(ret);
}

CAMLprim value ocaml_avcodec_set_packet_position(value _packet,
                                                 value _position) {
  CAMLparam2(_packet, _position);
  AVPacket *packet = Packet_val(_packet);

  if (_position == Val_none)
    packet->pos = -1;
  else
    packet->pos = Int64_val(Field(_position, 0));

  CAMLreturn(Val_unit);
}

CAMLprim value ocaml_avcodec_get_packet_dts(value _packet) {
  CAMLparam1(_packet);
  CAMLlocal1(ret);
  AVPacket *packet = Packet_val(_packet);

  if (packet->dts == AV_NOPTS_VALUE)
    CAMLreturn(Val_none);

  ret = caml_alloc_tuple(1);
  Store_field(ret, 0, caml_copy_int64(packet->dts));

  CAMLreturn(ret);
}

CAMLprim value ocaml_avcodec_set_packet_dts(value _packet, value _dts) {
  CAMLparam2(_packet, _dts);
  AVPacket *packet = Packet_val(_packet);

  if (_dts == Val_none)
    packet->dts = AV_NOPTS_VALUE;
  else
    packet->dts = Int64_val(Field(_dts, 0));

  CAMLreturn(Val_unit);
}

CAMLprim value ocaml_avcodec_get_packet_size(value _packet) {
  CAMLparam1(_packet);
  CAMLreturn(Val_int(Packet_val(_packet)->size));
}

CAMLprim value ocaml_avcodec_packet_to_bytes(value _packet) {
  CAMLparam1(_packet);
  CAMLlocal1(ans);
  struct AVPacket *packet = Packet_val(_packet);

  ans = caml_alloc_string(packet->size);

  memcpy((uint8_t *)String_val(ans), packet->data, packet->size);

  CAMLreturn(ans);
}

/***** AVCodecParserContext *****/

typedef struct {
  AVCodecParserContext *context;
  AVCodecContext *codec_context;
} parser_t;

#define Parser_val(v) (*(parser_t **)Data_custom_val(v))

static void free_parser(parser_t *parser) {
  if (!parser)
    return;

  caml_release_runtime_system();
  if (parser->context)
    av_parser_close(parser->context);

  if (parser->codec_context)
    avcodec_free_context(&parser->codec_context);
  caml_acquire_runtime_system();

  free(parser);
}

static void finalize_parser(value v) { free_parser(Parser_val(v)); }

static struct custom_operations parser_ops = {
    "ocaml_avcodec_parser",   finalize_parser,
    custom_compare_default,   custom_hash_default,
    custom_serialize_default, custom_deserialize_default};

static parser_t *create_parser(AVCodecParameters *params, AVCodec *codec) {
  parser_t *parser = (parser_t *)calloc(1, sizeof(parser_t));
  if (!parser)
    caml_raise_out_of_memory();

  caml_release_runtime_system();
  parser->context = av_parser_init(codec->id);
  caml_acquire_runtime_system();

  if (!parser->context) {
    free_parser(parser);
    caml_raise_out_of_memory();
  }

  parser->codec_context = create_AVCodecContext(NULL, codec);

  return parser;
}

CAMLprim value ocaml_avcodec_create_parser(value _params, value _codec) {
  CAMLparam1(_params);
  CAMLlocal1(ans);
  AVCodec *codec = (AVCodec *)_codec;
  AVCodecParameters *params = NULL;

  if (_params != Val_none)
    params = CodecParameters_val(Field(_params, 0));

  parser_t *parser = create_parser(params, codec);

  ans = caml_alloc_custom(&parser_ops, sizeof(parser_t *), 0, 1);
  Parser_val(ans) = parser;

  CAMLreturn(ans);
}

CAMLprim value ocaml_avcodec_parse_packet(value _parser, value _data,
                                          value _ofs, value _len) {
  CAMLparam2(_parser, _data);
  CAMLlocal3(val_packet, tuple, ans);
  parser_t *parser = Parser_val(_parser);
  uint8_t *data = Caml_ba_data_val(_data) + Int_val(_ofs);
  size_t init_len = Int_val(_len);
  size_t len = init_len;
  int ret = 0;

  caml_release_runtime_system();
  AVPacket *packet = av_packet_alloc();
  caml_acquire_runtime_system();

  if (!packet)
    caml_raise_out_of_memory();

  caml_release_runtime_system();

  do {
    ret = av_parser_parse2(parser->context, parser->codec_context,
                           &packet->data, &packet->size, data, len,
                           AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
    data += ret;
    len -= ret;
  } while (packet->size == 0 && ret > 0);

  if (ret < 0) {
    av_packet_free(&packet);
    caml_acquire_runtime_system();
    ocaml_avutil_raise_error(ret);
  }

  caml_acquire_runtime_system();

  if (packet->size) {
    val_packet = value_of_ffmpeg_packet(packet);

    tuple = caml_alloc_tuple(2);

    Store_field(tuple, 0, val_packet);
    Store_field(tuple, 1, Val_int(init_len - len));

    ans = caml_alloc(1, 0);
    Store_field(ans, 0, tuple);
  } else {
    caml_release_runtime_system();
    av_packet_free(&packet);
    caml_acquire_runtime_system();

    ans = Val_int(0);
  }

  CAMLreturn(ans);
}

/***** codec_context_t *****/

typedef struct {
  AVCodec *codec;
  AVCodecContext *codec_context;
  // output
  int flushed;
} codec_context_t;

#define CodecContext_val(v) (*(codec_context_t **)Data_custom_val(v))

static void codec_context(value v) {
  codec_context_t *ctx = CodecContext_val(v);
  if (ctx->codec_context)
    avcodec_free_context(&ctx->codec_context);

  free(ctx);
}

static struct custom_operations codec_context_ops = {
    "ocaml_codec_context",    codec_context,
    custom_compare_default,   custom_hash_default,
    custom_serialize_default, custom_deserialize_default};

CAMLprim value ocaml_avcodec_create_decoder(value _params, value _codec) {
  CAMLparam1(_params);
  CAMLlocal1(ans);
  AVCodec *codec = (AVCodec *)_codec;
  AVCodecParameters *params = NULL;

  if (_params != Val_none)
    params = CodecParameters_val(Field(_params, 0));

  codec_context_t *ctx = (codec_context_t *)calloc(1, sizeof(codec_context_t));
  if (!ctx)
    caml_raise_out_of_memory();

  ans = caml_alloc_custom(&codec_context_ops, sizeof(codec_context_t *), 0, 1);
  CodecContext_val(ans) = ctx;

  ctx->codec = codec;
  ctx->codec_context = create_AVCodecContext(params, ctx->codec);

  CAMLreturn(ans);
}

CAMLprim value ocaml_avcodec_sample_format(value _ctx) {
  CAMLparam1(_ctx);
  codec_context_t *ctx = CodecContext_val(_ctx);
  CAMLreturn(Val_SampleFormat(ctx->codec_context->sample_fmt));
}

CAMLprim value ocaml_avcodec_encoder_params(value _encoder) {
  CAMLparam1(_encoder);
  CAMLlocal1(ans);
  AVCodecParameters *params = avcodec_parameters_alloc();

  if (!params)
    caml_raise_out_of_memory();

  codec_context_t *ctx = CodecContext_val(_encoder);

  caml_release_runtime_system();
  int err = avcodec_parameters_from_context(params, ctx->codec_context);
  caml_acquire_runtime_system();

  if (err < 0) {
    avcodec_parameters_free(&params);
    ocaml_avutil_raise_error(err);
  }

  value_of_codec_parameters_copy(params, &ans);

  avcodec_parameters_free(&params);

  CAMLreturn(ans);
}

CAMLprim value ocaml_avcodec_encoder_time_base(value _encoder) {
  CAMLparam1(_encoder);
  CAMLlocal1(ans);
  codec_context_t *ctx = CodecContext_val(_encoder);

  value_of_rational(&ctx->codec_context->time_base, &ans);

  CAMLreturn(ans);
}

CAMLprim value ocaml_avcodec_create_audio_encoder(value _sample_fmt,
                                                  value _codec, value _opts) {
  CAMLparam1(_opts);
  CAMLlocal3(ret, ans, unused);
  AVCodec *codec = (AVCodec *)_codec;

  AVDictionary *options = NULL;
  char *key, *val;
  int len = Wosize_val(_opts);
  int i, err, count;

  for (i = 0; i < len; i++) {
    // Dictionaries copy key/values by default!
    key = (char *)Bytes_val(Field(Field(_opts, i), 0));
    val = (char *)Bytes_val(Field(Field(_opts, i), 1));
    err = av_dict_set(&options, key, val, 0);
    if (err < 0) {
      av_dict_free(&options);
      ocaml_avutil_raise_error(err);
    }
  }

  codec_context_t *ctx = (codec_context_t *)calloc(1, sizeof(codec_context_t));
  if (!ctx)
    caml_raise_out_of_memory();

  ans = caml_alloc_custom(&codec_context_ops, sizeof(codec_context_t *), 0, 1);
  CodecContext_val(ans) = ctx;

  ctx->codec = codec;

  caml_release_runtime_system();
  ctx->codec_context = avcodec_alloc_context3(codec);

  if (!ctx->codec_context) {
    caml_acquire_runtime_system();
    caml_raise_out_of_memory();
  }

  ctx->codec_context->sample_fmt = Int_val(_sample_fmt);

  // Open the codec
  err = avcodec_open2(ctx->codec_context, ctx->codec, &options);
  caml_acquire_runtime_system();

  if (err < 0)
    ocaml_avutil_raise_error(err);

  // Return unused keys
  caml_release_runtime_system();
  count = av_dict_count(options);
  caml_acquire_runtime_system();

  unused = caml_alloc_tuple(count);
  AVDictionaryEntry *entry = NULL;
  for (i = 0; i < count; i++) {
    entry = av_dict_get(options, "", entry, AV_DICT_IGNORE_SUFFIX);
    Store_field(unused, i, caml_copy_string(entry->key));
  }

  av_dict_free(&options);

  ret = caml_alloc_tuple(2);
  Store_field(ret, 0, ans);
  Store_field(ret, 1, unused);

  CAMLreturn(ret);
}

CAMLprim value ocaml_avcodec_create_video_encoder(value _device_context,
                                                  value _frame_context,
                                                  value _pix_fmt, value _codec,
                                                  value _opts) {
  CAMLparam3(_device_context, _frame_context, _codec);
  CAMLlocal3(ret, ans, unused);
  AVCodec *codec = (AVCodec *)_codec;

  AVBufferRef *device_ctx = NULL;
  AVBufferRef *frame_ctx = NULL;

  if (_device_context != Val_none)
    device_ctx = BufferRef_val(Some_val(_device_context));

  if (_frame_context != Val_none)
    frame_ctx = BufferRef_val(Some_val(_frame_context));

  AVDictionary *options = NULL;
  char *key, *val;
  int len = Wosize_val(_opts);
  int i, err, count;

  for (i = 0; i < len; i++) {
    // Dictionaries copy key/values by default!
    key = (char *)Bytes_val(Field(Field(_opts, i), 0));
    val = (char *)Bytes_val(Field(Field(_opts, i), 1));
    err = av_dict_set(&options, key, val, 0);
    if (err < 0) {
      av_dict_free(&options);
      ocaml_avutil_raise_error(err);
    }
  }

  codec_context_t *ctx = (codec_context_t *)calloc(1, sizeof(codec_context_t));
  if (!ctx) {
    av_dict_free(&options);
    caml_raise_out_of_memory();
  }

  ans = caml_alloc_custom(&codec_context_ops, sizeof(codec_context_t *), 0, 1);
  CodecContext_val(ans) = ctx;

  ctx->codec = codec;
  caml_release_runtime_system();
  ctx->codec_context = avcodec_alloc_context3(codec);
  caml_acquire_runtime_system();

  if (!ctx->codec_context) {
    caml_acquire_runtime_system();
    av_dict_free(&options);
    caml_raise_out_of_memory();
  }

  ctx->codec_context->pix_fmt = Int_val(_pix_fmt);

  if (device_ctx) {
    ctx->codec_context->hw_device_ctx = av_buffer_ref(device_ctx);
    if (!ctx->codec_context->hw_device_ctx) {
      av_dict_free(&options);
      caml_acquire_runtime_system();
      caml_raise_out_of_memory();
    }
  }

  if (frame_ctx) {
    ctx->codec_context->hw_frames_ctx = av_buffer_ref(frame_ctx);
    if (!ctx->codec_context->hw_frames_ctx) {
      av_dict_free(&options);
      caml_acquire_runtime_system();
      caml_raise_out_of_memory();
    }
  }

  // Open the codec
  caml_release_runtime_system();
  err = avcodec_open2(ctx->codec_context, ctx->codec, &options);
  caml_acquire_runtime_system();

  if (err < 0) {
    av_dict_free(&options);
    ocaml_avutil_raise_error(err);
  }

  // Return unused keys
  caml_release_runtime_system();
  count = av_dict_count(options);
  caml_acquire_runtime_system();

  unused = caml_alloc_tuple(count);
  AVDictionaryEntry *entry = NULL;
  for (i = 0; i < count; i++) {
    entry = av_dict_get(options, "", entry, AV_DICT_IGNORE_SUFFIX);
    Store_field(unused, i, caml_copy_string(entry->key));
  }

  av_dict_free(&options);

  ret = caml_alloc_tuple(2);
  Store_field(ret, 0, ans);
  Store_field(ret, 1, unused);

  CAMLreturn(ret);
}

CAMLprim value ocaml_avcodec_frame_size(value _ctx) {
  CAMLparam1(_ctx);
  codec_context_t *ctx = CodecContext_val(_ctx);
  CAMLreturn(Val_int(ctx->codec_context->frame_size));
}

CAMLprim value ocaml_avcodec_send_packet(value _ctx, value _packet) {
  CAMLparam2(_ctx, _packet);
  codec_context_t *ctx = CodecContext_val(_ctx);
  AVPacket *packet = _packet ? Packet_val(_packet) : NULL;

  // send the packet with the compressed data to the decoder
  caml_release_runtime_system();
  int ret = avcodec_send_packet(ctx->codec_context, packet);
  caml_acquire_runtime_system();

  if (ret < 0)
    ocaml_avutil_raise_error(ret);

  CAMLreturn(Val_unit);
}

CAMLprim value ocaml_avcodec_receive_frame(value _ctx) {
  CAMLparam1(_ctx);
  CAMLlocal2(val_frame, ans);
  codec_context_t *ctx = CodecContext_val(_ctx);
  int ret = 0;

  caml_release_runtime_system();
  AVFrame *frame = av_frame_alloc();

  if (!frame) {
    caml_acquire_runtime_system();
    caml_raise_out_of_memory();
  }

  if (ctx->codec_context->hw_frames_ctx && frame) {
    AVFrame *hw_frame = av_frame_alloc();

    if (!hw_frame) {
      caml_acquire_runtime_system();
      caml_raise_out_of_memory();
    }

    ret = av_hwframe_get_buffer(ctx->codec_context->hw_frames_ctx, hw_frame, 0);

    if (ret < 0) {
      av_frame_free(&hw_frame);
      caml_acquire_runtime_system();
      ocaml_avutil_raise_error(ret);
    }

    if (!hw_frame->hw_frames_ctx) {
      caml_acquire_runtime_system();
      caml_raise_out_of_memory();
    }

    ret = av_hwframe_transfer_data(hw_frame, frame, 0);

    if (ret < 0) {
      av_frame_free(&hw_frame);
      caml_acquire_runtime_system();
      ocaml_avutil_raise_error(ret);
    }

    frame = hw_frame;
  }

  ret = avcodec_receive_frame(ctx->codec_context, frame);

  if (ret < 0 && ret != AVERROR(EAGAIN)) {
    av_frame_free(&frame);
    caml_acquire_runtime_system();
    ocaml_avutil_raise_error(ret);
  }

  caml_acquire_runtime_system();

  if (ret == AVERROR(EAGAIN)) {
    ans = Val_int(0);
  } else {
    ans = caml_alloc(1, 0);
    val_frame = value_of_frame(frame);
    Store_field(ans, 0, val_frame);
  }

  CAMLreturn(ans);
}

CAMLprim value ocaml_avcodec_flush_decoder(value _ctx) {
  ocaml_avcodec_send_packet(_ctx, 0);
  return Val_unit;
}

static void send_frame(codec_context_t *ctx, AVFrame *frame) {
  int ret;
  AVFrame *hw_frame = NULL;

  ctx->flushed = !frame;

  if (ctx->codec_context->hw_frames_ctx && frame) {
    hw_frame = av_frame_alloc();

    if (!hw_frame) {
      caml_acquire_runtime_system();
      caml_raise_out_of_memory();
    }

    ret = av_hwframe_get_buffer(ctx->codec_context->hw_frames_ctx, hw_frame, 0);

    if (ret < 0) {
      av_frame_free(&hw_frame);
      caml_acquire_runtime_system();
      ocaml_avutil_raise_error(ret);
    }

    if (!hw_frame->hw_frames_ctx) {
      caml_acquire_runtime_system();
      caml_raise_out_of_memory();
    }

    ret = av_hwframe_transfer_data(hw_frame, frame, 0);

    if (ret < 0) {
      av_frame_free(&hw_frame);
      caml_acquire_runtime_system();
      ocaml_avutil_raise_error(ret);
    }

    frame = hw_frame;
  }

  caml_release_runtime_system();
  ret = avcodec_send_frame(ctx->codec_context, frame);
  caml_acquire_runtime_system();

  if (hw_frame)
    av_frame_free(&hw_frame);

  if (ret < 0)
    ocaml_avutil_raise_error(ret);
}

CAMLprim value ocaml_avcodec_send_frame(value _ctx, value _frame) {
  CAMLparam2(_ctx, _frame);
  CAMLlocal1(val_packet);
  codec_context_t *ctx = CodecContext_val(_ctx);
  AVFrame *frame = _frame ? Frame_val(_frame) : NULL;

  send_frame(ctx, frame);

  CAMLreturn(Val_unit);
}

CAMLprim value ocaml_avcodec_receive_packet(value _ctx) {
  CAMLparam1(_ctx);
  CAMLlocal2(val_packet, ans);
  codec_context_t *ctx = CodecContext_val(_ctx);
  int ret = 0;

  caml_release_runtime_system();
  AVPacket *packet = av_packet_alloc();
  caml_acquire_runtime_system();

  if (!packet)
    caml_raise_out_of_memory();

  caml_release_runtime_system();
  ret = avcodec_receive_packet(ctx->codec_context, packet);
  caml_acquire_runtime_system();

  if (ret < 0) {
    caml_release_runtime_system();
    av_packet_free(&packet);
    caml_acquire_runtime_system();

    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
      ans = Val_int(0);
    else
      ocaml_avutil_raise_error(ret);
  } else {
    ans = caml_alloc(1, 0);
    val_packet = value_of_ffmpeg_packet(packet);
    Store_field(ans, 0, val_packet);
  }
  CAMLreturn(ans);
}

CAMLprim value ocaml_avcodec_flush_encoder(value _ctx) {
  ocaml_avcodec_send_frame(_ctx, 0);
  return Val_unit;
}

/**** codec ****/

static AVCodec *find_encoder_by_name(const char *name, enum AVMediaType type) {
  caml_release_runtime_system();
  AVCodec *codec = avcodec_find_encoder_by_name(name);
  caml_acquire_runtime_system();

  if (!codec || codec->type != type)
    ocaml_avutil_raise_error(AVERROR_ENCODER_NOT_FOUND);

  return codec;
}

static AVCodec *find_encoder(enum AVCodecID id, enum AVMediaType type) {
  caml_release_runtime_system();
  AVCodec *codec = avcodec_find_encoder(id);
  caml_acquire_runtime_system();

  if (!codec || codec->type != type)
    ocaml_avutil_raise_error(AVERROR_ENCODER_NOT_FOUND);

  return codec;
}

static AVCodec *find_decoder_by_name(const char *name, enum AVMediaType type) {
  caml_release_runtime_system();
  AVCodec *codec = avcodec_find_decoder_by_name(name);
  caml_acquire_runtime_system();

  if (!codec || codec->type != type)
    ocaml_avutil_raise_error(AVERROR_DECODER_NOT_FOUND);

  return codec;
}

static AVCodec *find_decoder(enum AVCodecID id, enum AVMediaType type) {
  caml_release_runtime_system();
  AVCodec *codec = avcodec_find_decoder(id);
  caml_acquire_runtime_system();

  if (!codec || codec->type != type)
    ocaml_avutil_raise_error(AVERROR_DECODER_NOT_FOUND);

  return codec;
}

CAMLprim value ocaml_avcodec_parameters_get_bit_rate(value _cp) {
  CAMLparam1(_cp);
  CAMLreturn(Val_int(CodecParameters_val(_cp)->bit_rate));
}

/**** Audio codec ID ****/

CAMLprim value ocaml_avcodec_get_audio_codec_id(value _codec) {
  CAMLparam0();
  AVCodec *codec = (AVCodec *)_codec;
  CAMLreturn(value_of_audio_codec_id(codec->id));
}

CAMLprim value ocaml_avcodec_get_video_codec_id(value _codec) {
  CAMLparam0();
  AVCodec *codec = (AVCodec *)_codec;
  CAMLreturn(value_of_video_codec_id(codec->id));
}

CAMLprim value ocaml_avcodec_get_subtitle_codec_id(value _codec) {
  CAMLparam0();
  AVCodec *codec = (AVCodec *)_codec;
  CAMLreturn(value_of_subtitle_codec_id(codec->id));
}

CAMLprim value ocaml_avcodec_get_audio_codec_id_name(value _codec_id) {
  CAMLparam1(_codec_id);
  CAMLreturn(caml_copy_string(
      avcodec_get_name((enum AVCodecID)AudioCodecID_val(_codec_id))));
}

CAMLprim value ocaml_avcodec_find_audio_encoder_by_name(value _name) {
  CAMLparam1(_name);
  CAMLreturn(
      (value)find_encoder_by_name(String_val(_name), AVMEDIA_TYPE_AUDIO));
}

CAMLprim value ocaml_avcodec_find_audio_encoder(value _id) {
  CAMLparam1(_id);
  CAMLreturn((value)find_encoder(AudioCodecID_val(_id), AVMEDIA_TYPE_AUDIO));
}

CAMLprim value ocaml_avcodec_find_audio_decoder_by_name(value _name) {
  CAMLparam1(_name);
  CAMLreturn(
      (value)find_decoder_by_name(String_val(_name), AVMEDIA_TYPE_AUDIO));
}

CAMLprim value ocaml_avcodec_find_audio_decoder(value _id) {
  CAMLparam1(_id);
  CAMLreturn((value)find_decoder(AudioCodecID_val(_id), AVMEDIA_TYPE_AUDIO));
}

CAMLprim value ocaml_avcodec_name(value _codec) {
  CAMLparam0();
  CAMLreturn(caml_copy_string(((AVCodec *)_codec)->name));
}

CAMLprim value ocaml_avcodec_capabilities(value _codec) {
  CAMLparam0();
  CAMLlocal1(ret);
  AVCodec *codec = (AVCodec *)_codec;
  int i, len;

  len = 0;
  for (i = 0; i < AV_CODEC_CAP_T_TAB_LEN; i++)
    if (codec->capabilities & AV_CODEC_CAP_T_TAB[i][1])
      len++;

  ret = caml_alloc_tuple(len);

  len = 0;
  for (i = 0; i < AV_CODEC_CAP_T_TAB_LEN; i++)
    if (codec->capabilities & AV_CODEC_CAP_T_TAB[i][1])
      Store_field(ret, len++, Val_int(AV_CODEC_CAP_T_TAB[i][0]));

  CAMLreturn(ret);
}

CAMLprim value ocaml_avcodec_hw_methods(value _codec) {
  CAMLparam0();
  CAMLlocal5(ret, tmp1, cons1, tmp2, cons2);
  AVCodec *codec = (AVCodec *)_codec;
  int n, i = 0;
  const AVCodecHWConfig *config = avcodec_get_hw_config(codec, i);

  if (!config)
    CAMLreturn(Val_int(0));

  cons1 = Val_int(0);
  do {
    ret = caml_alloc(2, 0);
    Store_field(ret, 1, cons1);

    tmp1 = caml_alloc_tuple(3);
    Store_field(tmp1, 0, Val_PixelFormat(config->pix_fmt));

    tmp2 = Val_int(0);
    cons2 = Val_int(0);
    for (n = 0; n < AV_CODEC_HW_CONFIG_METHOD_T_TAB_LEN; n++) {
      if (config->methods & AV_CODEC_HW_CONFIG_METHOD_T_TAB[n][1]) {
        tmp2 = caml_alloc(2, 0);
        Store_field(tmp2, 0, AV_CODEC_HW_CONFIG_METHOD_T_TAB[n][0]);
        Store_field(tmp2, 1, cons2);
        cons2 = tmp2;
      }
    }
    Store_field(tmp1, 1, tmp2);

    Store_field(tmp1, 2, Val_HwDeviceType(config->device_type));

    Store_field(ret, 0, tmp1);
    cons1 = ret;
    i++;
    config = avcodec_get_hw_config(codec, i);
  } while (config);

  CAMLreturn(ret);
}

CAMLprim value ocaml_avcodec_get_supported_channel_layouts(value _codec) {
  CAMLparam0();
  CAMLlocal2(list, cons);
  int i;
  List_init(list);
  AVCodec *codec = (AVCodec *)_codec;

  if (codec->channel_layouts) {
    for (i = 0; codec->channel_layouts[i] != -1; i++)
      List_add(list, cons, Val_ChannelLayout(codec->channel_layouts[i]));
  }

  CAMLreturn(list);
}

CAMLprim value ocaml_avcodec_get_supported_sample_formats(value _codec) {
  CAMLparam0();
  CAMLlocal2(list, cons);
  int i;
  List_init(list);
  AVCodec *codec = (AVCodec *)_codec;

  if (codec->sample_fmts) {
    for (i = 0; codec->sample_fmts[i] != -1; i++)
      List_add(list, cons, Val_SampleFormat(codec->sample_fmts[i]));
  }

  CAMLreturn(list);
}

CAMLprim value ocaml_avcodec_get_supported_sample_rates(value _codec) {
  CAMLparam0();
  CAMLlocal2(list, cons);
  int i;
  List_init(list);
  AVCodec *codec = (AVCodec *)_codec;

  if (codec->supported_samplerates) {
    for (i = 0; codec->supported_samplerates[i] != 0; i++)
      List_add(list, cons, Val_int(codec->supported_samplerates[i]));
  }

  CAMLreturn(list);
}

/**** Audio codec parameters ****/
CAMLprim value ocaml_avcodec_parameters_get_audio_codec_id(value _cp) {
  CAMLparam1(_cp);
  CAMLreturn(value_of_audio_codec_id(CodecParameters_val(_cp)->codec_id));
}

CAMLprim value ocaml_avcodec_parameters_get_channel_layout(value _cp) {
  CAMLparam1(_cp);
  AVCodecParameters *cp = CodecParameters_val(_cp);

  if (cp->channel_layout == 0) {
    cp->channel_layout = av_get_default_channel_layout(cp->channels);
  }

  CAMLreturn(Val_ChannelLayout(cp->channel_layout));
}

CAMLprim value ocaml_avcodec_parameters_get_nb_channels(value _cp) {
  CAMLparam1(_cp);
  CAMLreturn(Val_int(CodecParameters_val(_cp)->channels));
}

CAMLprim value ocaml_avcodec_parameters_get_sample_format(value _cp) {
  CAMLparam1(_cp);
  CAMLreturn(
      Val_SampleFormat((enum AVSampleFormat)CodecParameters_val(_cp)->format));
}

CAMLprim value ocaml_avcodec_parameters_get_sample_rate(value _cp) {
  CAMLparam1(_cp);
  CAMLreturn(Val_int(CodecParameters_val(_cp)->sample_rate));
}

CAMLprim value ocaml_avcodec_parameters_audio_copy(value _codec_id,
                                                   value _channel_layout,
                                                   value _sample_format,
                                                   value _sample_rate,
                                                   value _cp) {
  CAMLparam4(_codec_id, _channel_layout, _sample_format, _cp);
  CAMLlocal1(ans);

  value_of_codec_parameters_copy(CodecParameters_val(_cp), &ans);

  AVCodecParameters *dst = CodecParameters_val(ans);

  dst->codec_id = AudioCodecID_val(_codec_id);
  dst->channel_layout = ChannelLayout_val(_channel_layout);
  dst->channels = av_get_channel_layout_nb_channels(dst->channel_layout);
  dst->format = SampleFormat_val(_sample_format);
  dst->sample_rate = Int_val(_sample_rate);

  CAMLreturn(ans);
}

/**** Video codec ID ****/

CAMLprim value ocaml_avcodec_get_video_codec_id_name(value _codec_id) {
  CAMLparam1(_codec_id);
  CAMLreturn(caml_copy_string(avcodec_get_name(VideoCodecID_val(_codec_id))));
}

CAMLprim value ocaml_avcodec_find_video_decoder_by_name(value _name) {
  CAMLparam1(_name);
  CAMLreturn(
      (value)find_decoder_by_name(String_val(_name), AVMEDIA_TYPE_VIDEO));
}

CAMLprim value ocaml_avcodec_find_video_decoder(value _id) {
  CAMLparam1(_id);
  CAMLreturn((value)find_decoder(VideoCodecID_val(_id), AVMEDIA_TYPE_VIDEO));
}

CAMLprim value ocaml_avcodec_find_video_encoder_by_name(value _name) {
  CAMLparam1(_name);
  CAMLreturn(
      (value)find_encoder_by_name(String_val(_name), AVMEDIA_TYPE_VIDEO));
}

CAMLprim value ocaml_avcodec_find_video_encoder(value _id) {
  CAMLparam1(_id);
  CAMLreturn((value)find_encoder(VideoCodecID_val(_id), AVMEDIA_TYPE_VIDEO));
}

CAMLprim value ocaml_avcodec_get_supported_frame_rates(value _codec) {
  CAMLparam0();
  CAMLlocal3(list, cons, val);
  int i;
  List_init(list);
  AVCodec *codec = (AVCodec *)_codec;

  if (codec->supported_framerates) {
    for (i = 0; codec->supported_framerates[i].num != 0; i++) {
      value_of_rational(&codec->supported_framerates[i], &val);
      List_add(list, cons, val);
    }
  }

  CAMLreturn(list);
}

CAMLprim value ocaml_avcodec_get_supported_pixel_formats(value _codec) {
  CAMLparam0();
  CAMLlocal2(list, cons);
  int i;
  List_init(list);
  AVCodec *codec = (AVCodec *)_codec;

  if (codec->pix_fmts) {
    for (i = 0; codec->pix_fmts[i] != -1; i++)
      List_add(list, cons, Val_PixelFormat(codec->pix_fmts[i]));
  }

  CAMLreturn(list);
}

/**** Video codec parameters ****/
CAMLprim value ocaml_avcodec_parameters_get_video_codec_id(value _cp) {
  CAMLparam1(_cp);
  CAMLreturn(value_of_video_codec_id(CodecParameters_val(_cp)->codec_id));
}

CAMLprim value ocaml_avcodec_parameters_get_width(value _cp) {
  CAMLparam1(_cp);
  CAMLreturn(Val_int(CodecParameters_val(_cp)->width));
}

CAMLprim value ocaml_avcodec_parameters_get_height(value _cp) {
  CAMLparam1(_cp);
  CAMLreturn(Val_int(CodecParameters_val(_cp)->height));
}

CAMLprim value ocaml_avcodec_parameters_get_sample_aspect_ratio(value _cp) {
  CAMLparam1(_cp);
  CAMLlocal1(ans);

  value_of_rational(&CodecParameters_val(_cp)->sample_aspect_ratio, &ans);

  CAMLreturn(ans);
}

CAMLprim value ocaml_avcodec_parameters_get_pixel_format(value _cp) {
  CAMLparam1(_cp);
  CAMLlocal1(ret);
  enum AVPixelFormat f = CodecParameters_val(_cp)->format;

  if (f == AV_PIX_FMT_NONE)
    CAMLreturn(Val_none);

  ret = caml_alloc_tuple(1);
  Store_field(ret, 0, Val_PixelFormat(f));

  CAMLreturn(ret);
}

CAMLprim value ocaml_avcodec_parameters_get_pixel_aspect(value _cp) {
  CAMLparam1(_cp);
  CAMLlocal2(ret, ans);
  const AVRational pixel_aspect = CodecParameters_val(_cp)->sample_aspect_ratio;

  if (pixel_aspect.num == 0)
    CAMLreturn(Val_none);

  value_of_rational(&pixel_aspect, &ans);

  ret = caml_alloc_tuple(1);
  Store_field(ret, 0, ans);

  CAMLreturn(ret);
}

CAMLprim value ocaml_avcodec_parameters_video_copy(value _codec_id,
                                                   value _width, value _height,
                                                   value _sample_aspect_ratio,
                                                   value _pixel_format,
                                                   value _bit_rate, value _cp) {
  CAMLparam4(_codec_id, _sample_aspect_ratio, _pixel_format, _cp);
  CAMLlocal1(ans);

  value_of_codec_parameters_copy(CodecParameters_val(_cp), &ans);

  AVCodecParameters *dst = CodecParameters_val(ans);

  dst->codec_id = VideoCodecID_val(_codec_id);
  dst->width = Int_val(_width);
  dst->height = Int_val(_height);
  dst->sample_aspect_ratio.num = Int_val(Field(_sample_aspect_ratio, 0));
  dst->sample_aspect_ratio.den = Int_val(Field(_sample_aspect_ratio, 1));
  dst->format = PixelFormat_val(_pixel_format);
  dst->bit_rate = Int_val(_bit_rate);

  CAMLreturn(ans);
}

CAMLprim value ocaml_avcodec_parameters_video_copy_byte(value *argv, int argn) {
  return ocaml_avcodec_parameters_video_copy(argv[0], argv[1], argv[2], argv[3],
                                             argv[4], argv[5], argv[7]);
}

/**** Subtitle codec ID ****/

CAMLprim value ocaml_avcodec_get_subtitle_codec_id_name(value _codec_id) {
  CAMLparam1(_codec_id);
  CAMLreturn(
      caml_copy_string(avcodec_get_name(SubtitleCodecID_val(_codec_id))));
}

CAMLprim value ocaml_avcodec_find_subtitle_decoder_by_name(value _name) {
  CAMLparam1(_name);
  CAMLreturn(
      (value)find_decoder_by_name(String_val(_name), AVMEDIA_TYPE_SUBTITLE));
}

CAMLprim value ocaml_avcodec_find_subtitle_decoder(value _id) {
  CAMLparam1(_id);
  CAMLreturn(
      (value)find_decoder(SubtitleCodecID_val(_id), AVMEDIA_TYPE_SUBTITLE));
}

CAMLprim value ocaml_avcodec_find_subtitle_encoder_by_name(value _name) {
  CAMLparam1(_name);
  CAMLreturn(
      (value)find_encoder_by_name(String_val(_name), AVMEDIA_TYPE_SUBTITLE));
}

CAMLprim value ocaml_avcodec_find_subtitle_encoder(value _id) {
  CAMLparam1(_id);
  CAMLreturn(
      (value)find_encoder(SubtitleCodecID_val(_id), AVMEDIA_TYPE_SUBTITLE));
}

/**** Subtitle codec parameters ****/
CAMLprim value ocaml_avcodec_parameters_get_subtitle_codec_id(value _cp) {
  CAMLparam1(_cp);
  CAMLreturn(value_of_subtitle_codec_id(CodecParameters_val(_cp)->codec_id));
}

CAMLprim value ocaml_avcodec_parameters_subtitle_copy(value _codec_id,
                                                      value _cp) {
  CAMLparam2(_codec_id, _cp);
  CAMLlocal1(ans);

  value_of_codec_parameters_copy(CodecParameters_val(_cp), &ans);

  AVCodecParameters *dst = CodecParameters_val(ans);

  dst->codec_id = SubtitleCodecID_val(_codec_id);

  CAMLreturn(ans);
}

CAMLprim value ocaml_avcodec_int_of_flag(value _flag) {
  CAMLparam1(_flag);

  switch (_flag) {
  case PVV_Keyframe:
    CAMLreturn(Val_int(AV_PKT_FLAG_KEY));
  case PVV_Corrupt:
    CAMLreturn(Val_int(AV_PKT_FLAG_CORRUPT));
  case PVV_Discard:
    CAMLreturn(Val_int(AV_PKT_FLAG_DISCARD));
  case PVV_Trusted:
    CAMLreturn(Val_int(AV_PKT_FLAG_TRUSTED));
  case PVV_Disposable:
    CAMLreturn(Val_int(AV_PKT_FLAG_DISPOSABLE));
  default:
    caml_failwith("Invalid flag type!");
  }
}
