#include <caml/mlvalues.h>
#include <caml/memory.h>
#include <caml/alloc.h>
#include <caml/fail.h>
#include <caml/callback.h>
/*
#include <caml/custom.h>
#include <caml/bigarray.h>
#include <caml/threads.h>

#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libavutil/timestamp.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include "libavutil/audio_fifo.h"
*/
#include <libavformat/avformat.h>
#include "avutil_stubs.h"

#include "avcodec_stubs.h"


static enum AVCodecID find_codec_id_by_name(const char *name)
{
  av_register_all();

  AVCodec * codec = avcodec_find_encoder_by_name(name);
  if( ! codec) Raise(EXN_FAILURE, "Failed to find %s codec", name);

  return codec->id;
}

/**** Audio codec ID ****/
static const enum AVCodecID AUDIO_CODEC_IDS[] = {
  AV_CODEC_ID_MP2,
  AV_CODEC_ID_MP3,
  AV_CODEC_ID_AAC,
  AV_CODEC_ID_AC3,
  AV_CODEC_ID_DTS,
  AV_CODEC_ID_VORBIS,
  AV_CODEC_ID_DVAUDIO,
  AV_CODEC_ID_WMAV1,
  AV_CODEC_ID_WMAV2,
  AV_CODEC_ID_MACE3,
  AV_CODEC_ID_MACE6,
  AV_CODEC_ID_VMDAUDIO,
  AV_CODEC_ID_FLAC,
  AV_CODEC_ID_MP3ADU,
  AV_CODEC_ID_MP3ON4,
  AV_CODEC_ID_SHORTEN,
  AV_CODEC_ID_ALAC,
  AV_CODEC_ID_WESTWOOD_SND1,
  AV_CODEC_ID_GSM,
  AV_CODEC_ID_QDM2,
  AV_CODEC_ID_COOK,
  AV_CODEC_ID_TRUESPEECH,
  AV_CODEC_ID_TTA,
  AV_CODEC_ID_SMACKAUDIO,
  AV_CODEC_ID_QCELP,
  AV_CODEC_ID_WAVPACK,
  AV_CODEC_ID_DSICINAUDIO,
  AV_CODEC_ID_IMC,
  AV_CODEC_ID_MUSEPACK7,
  AV_CODEC_ID_MLP,
  AV_CODEC_ID_GSM_MS,
  AV_CODEC_ID_ATRAC3,
  AV_CODEC_ID_APE,
  AV_CODEC_ID_NELLYMOSER,
  AV_CODEC_ID_MUSEPACK8,
  AV_CODEC_ID_SPEEX,
  AV_CODEC_ID_WMAVOICE,
  AV_CODEC_ID_WMAPRO,
  AV_CODEC_ID_WMALOSSLESS,
  AV_CODEC_ID_ATRAC3P,
  AV_CODEC_ID_EAC3,
  AV_CODEC_ID_SIPR,
  AV_CODEC_ID_MP1,
  AV_CODEC_ID_TWINVQ,
  AV_CODEC_ID_TRUEHD,
  AV_CODEC_ID_MP4ALS,
  AV_CODEC_ID_ATRAC1,
  AV_CODEC_ID_BINKAUDIO_RDFT,
  AV_CODEC_ID_BINKAUDIO_DCT,
  AV_CODEC_ID_AAC_LATM,
  AV_CODEC_ID_QDMC,
  AV_CODEC_ID_CELT,
  AV_CODEC_ID_G723_1,
  AV_CODEC_ID_G729,
  AV_CODEC_ID_8SVX_EXP,
  AV_CODEC_ID_8SVX_FIB,
  AV_CODEC_ID_BMV_AUDIO,
  AV_CODEC_ID_RALF,
  AV_CODEC_ID_IAC,
  AV_CODEC_ID_ILBC,
  AV_CODEC_ID_OPUS,
  AV_CODEC_ID_COMFORT_NOISE,
  AV_CODEC_ID_TAK,
  /*
  AV_CODEC_ID_METASOUND,
  AV_CODEC_ID_PAF_AUDIO,
  AV_CODEC_ID_ON2AVC,
  AV_CODEC_ID_DSS_SP,
  AV_CODEC_ID_FFWAVESYNTH,
  AV_CODEC_ID_SONIC,
  AV_CODEC_ID_SONIC_LS,
  AV_CODEC_ID_EVRC,
  AV_CODEC_ID_SMV,
  AV_CODEC_ID_DSD_LSBF,
  AV_CODEC_ID_DSD_MSBF,
  AV_CODEC_ID_DSD_LSBF_PLANAR,
  AV_CODEC_ID_DSD_MSBF_PLANAR,
  AV_CODEC_ID_4GV,
  AV_CODEC_ID_INTERPLAY_ACM,
  AV_CODEC_ID_XMA1,
  AV_CODEC_ID_XMA2,
  AV_CODEC_ID_DST
  */
};
#define AUDIO_CODEC_IDS_LEN (sizeof(AUDIO_CODEC_IDS) / sizeof(enum AVCodecID))

enum AVCodecID AudioCodecId_val(value v)
{
  return AUDIO_CODEC_IDS[Int_val(v)];
}

value Val_audioCodecId(enum AVCodecID id)
{
  int i;
  for (i = 0; i < AUDIO_CODEC_IDS_LEN; i++) {
    if (id == AUDIO_CODEC_IDS[i])
      return Val_int(i);
  }
  Raise(EXN_FAILURE, "Invalid audio codec id : %d\n", id);
  return Val_int(0);
}
/*
CAMLprim value ocaml_avcodec_get_audio_codec_name(value _codec_id)
{
  CAMLparam1(_codec_id);
  CAMLreturn(caml_copy_string(avcodec_get_name(AudioCodecId_val(_codec_id))));
}
*/  
CAMLprim value ocaml_avcodec_find_audio_codec_id_by_name(value _name)
{
  CAMLparam1(_name);
  CAMLreturn(Val_audioCodecId(find_codec_id_by_name(String_val(_name))));
}


CAMLprim value ocaml_avcodec_find_best_sample_format(value _audio_codec_id)
{
  CAMLparam1(_audio_codec_id);
  enum AVCodecID codec_id = AudioCodecId_val(_audio_codec_id);

  av_register_all();

  AVCodec * codec = avcodec_find_encoder(codec_id);
  if( ! codec) Raise(EXN_FAILURE, "Failed to find codec");
  if( ! codec->sample_fmts) Raise(EXN_FAILURE, "Failed to find codec sample formats");
    
  CAMLreturn(Val_sampleFormat(codec->sample_fmts[0]));
}


/**** Video codec ID ****/
static const enum AVCodecID VIDEO_CODEC_IDS[] = {
  AV_CODEC_ID_MPEG1VIDEO,
  AV_CODEC_ID_MPEG2VIDEO,
  AV_CODEC_ID_H261,
  AV_CODEC_ID_H263,
  AV_CODEC_ID_RV10,
  AV_CODEC_ID_RV20,
  AV_CODEC_ID_MJPEG,
  AV_CODEC_ID_MJPEGB,
  AV_CODEC_ID_LJPEG,
  AV_CODEC_ID_SP5X,
  AV_CODEC_ID_JPEGLS,
  AV_CODEC_ID_MPEG4,
  AV_CODEC_ID_RAWVIDEO,
  AV_CODEC_ID_MSMPEG4V1,
  AV_CODEC_ID_MSMPEG4V2,
  AV_CODEC_ID_MSMPEG4V3,
  AV_CODEC_ID_WMV1,
  AV_CODEC_ID_WMV2,
  AV_CODEC_ID_H263P,
  AV_CODEC_ID_H263I,
  AV_CODEC_ID_FLV1,
  AV_CODEC_ID_SVQ1,
  AV_CODEC_ID_SVQ3,
  AV_CODEC_ID_DVVIDEO,
  AV_CODEC_ID_HUFFYUV,
  AV_CODEC_ID_CYUV,
  AV_CODEC_ID_H264,
  AV_CODEC_ID_INDEO3,
  AV_CODEC_ID_VP3,
  AV_CODEC_ID_THEORA,
  AV_CODEC_ID_ASV1,
  AV_CODEC_ID_ASV2,
  AV_CODEC_ID_FFV1,
  AV_CODEC_ID_4XM,
  AV_CODEC_ID_VCR1,
  AV_CODEC_ID_CLJR,
  AV_CODEC_ID_MDEC,
  AV_CODEC_ID_ROQ,
  AV_CODEC_ID_INTERPLAY_VIDEO,
  AV_CODEC_ID_XAN_WC3,
  AV_CODEC_ID_XAN_WC4,
  AV_CODEC_ID_RPZA,
  AV_CODEC_ID_CINEPAK,
  AV_CODEC_ID_WS_VQA,
  AV_CODEC_ID_MSRLE,
  AV_CODEC_ID_MSVIDEO1,
  AV_CODEC_ID_IDCIN,
  AV_CODEC_ID_8BPS,
  AV_CODEC_ID_SMC,
  AV_CODEC_ID_FLIC,
  AV_CODEC_ID_TRUEMOTION1,
  AV_CODEC_ID_VMDVIDEO,
  AV_CODEC_ID_MSZH,
  AV_CODEC_ID_ZLIB,
  AV_CODEC_ID_QTRLE,
  AV_CODEC_ID_TSCC,
  AV_CODEC_ID_ULTI,
  AV_CODEC_ID_QDRAW,
  AV_CODEC_ID_VIXL,
  AV_CODEC_ID_QPEG,
  AV_CODEC_ID_PNG,
  AV_CODEC_ID_PPM,
  AV_CODEC_ID_PBM,
  AV_CODEC_ID_PGM,
  AV_CODEC_ID_PGMYUV,
  AV_CODEC_ID_PAM,
  AV_CODEC_ID_FFVHUFF,
  AV_CODEC_ID_RV30,
  AV_CODEC_ID_RV40,
  AV_CODEC_ID_VC1,
  AV_CODEC_ID_WMV3,
  AV_CODEC_ID_LOCO,
  AV_CODEC_ID_WNV1,
  AV_CODEC_ID_AASC,
  AV_CODEC_ID_INDEO2,
  AV_CODEC_ID_FRAPS,
  AV_CODEC_ID_TRUEMOTION2,
  AV_CODEC_ID_BMP,
  AV_CODEC_ID_CSCD,
  AV_CODEC_ID_MMVIDEO,
  AV_CODEC_ID_ZMBV,
  AV_CODEC_ID_AVS,
  AV_CODEC_ID_SMACKVIDEO,
  AV_CODEC_ID_NUV,
  AV_CODEC_ID_KMVC,
  AV_CODEC_ID_FLASHSV,
  AV_CODEC_ID_CAVS,
  AV_CODEC_ID_JPEG2000,
  AV_CODEC_ID_VMNC,
  AV_CODEC_ID_VP5,
  AV_CODEC_ID_VP6,
  AV_CODEC_ID_VP6F,
  AV_CODEC_ID_TARGA,
  AV_CODEC_ID_DSICINVIDEO,
  AV_CODEC_ID_TIERTEXSEQVIDEO,
  AV_CODEC_ID_TIFF,
  AV_CODEC_ID_GIF,
  AV_CODEC_ID_DXA,
  AV_CODEC_ID_DNXHD,
  AV_CODEC_ID_THP,
  AV_CODEC_ID_SGI,
  AV_CODEC_ID_C93,
  AV_CODEC_ID_BETHSOFTVID,
  AV_CODEC_ID_PTX,
  AV_CODEC_ID_TXD,
  AV_CODEC_ID_VP6A,
  AV_CODEC_ID_AMV,
  AV_CODEC_ID_VB,
  AV_CODEC_ID_PCX,
  AV_CODEC_ID_SUNRAST,
  AV_CODEC_ID_INDEO4,
  AV_CODEC_ID_INDEO5,
  AV_CODEC_ID_MIMIC,
  AV_CODEC_ID_RL2,
  AV_CODEC_ID_ESCAPE124,
  AV_CODEC_ID_DIRAC,
  AV_CODEC_ID_BFI,
  AV_CODEC_ID_CMV,
  AV_CODEC_ID_MOTIONPIXELS,
  AV_CODEC_ID_TGV,
  AV_CODEC_ID_TGQ,
  AV_CODEC_ID_TQI,
  AV_CODEC_ID_AURA,
  AV_CODEC_ID_AURA2,
  AV_CODEC_ID_V210X,
  AV_CODEC_ID_TMV,
  AV_CODEC_ID_V210,
  AV_CODEC_ID_DPX,
  AV_CODEC_ID_MAD,
  AV_CODEC_ID_FRWU,
  AV_CODEC_ID_FLASHSV2,
  AV_CODEC_ID_CDGRAPHICS,
  AV_CODEC_ID_R210,
  AV_CODEC_ID_ANM,
  AV_CODEC_ID_BINKVIDEO,
  AV_CODEC_ID_IFF_ILBM,
  AV_CODEC_ID_IFF_BYTERUN1,
  AV_CODEC_ID_KGV1,
  AV_CODEC_ID_YOP,
  AV_CODEC_ID_VP8,
  AV_CODEC_ID_PICTOR,
  AV_CODEC_ID_ANSI,
  AV_CODEC_ID_A64_MULTI,
  AV_CODEC_ID_A64_MULTI5,
  AV_CODEC_ID_R10K,
  AV_CODEC_ID_MXPEG,
  AV_CODEC_ID_LAGARITH,
  AV_CODEC_ID_PRORES,
  AV_CODEC_ID_JV,
  AV_CODEC_ID_DFA,
  AV_CODEC_ID_WMV3IMAGE,
  AV_CODEC_ID_VC1IMAGE,
  AV_CODEC_ID_UTVIDEO,
  AV_CODEC_ID_BMV_VIDEO,
  AV_CODEC_ID_VBLE,
  AV_CODEC_ID_DXTORY,
  AV_CODEC_ID_V410,
  AV_CODEC_ID_XWD,
  AV_CODEC_ID_CDXL,
  AV_CODEC_ID_XBM,
  AV_CODEC_ID_ZEROCODEC,
  AV_CODEC_ID_MSS1,
  AV_CODEC_ID_MSA1,
  AV_CODEC_ID_TSCC2,
  AV_CODEC_ID_MTS2,
  AV_CODEC_ID_CLLC,
  AV_CODEC_ID_MSS2,
  /*
  AV_CODEC_ID_VP9,
  AV_CODEC_ID_AIC,
  AV_CODEC_ID_ESCAPE130,
  AV_CODEC_ID_G2M,
  AV_CODEC_ID_WEBP,
  AV_CODEC_ID_HNM4_VIDEO,
  AV_CODEC_ID_HEVC,
  AV_CODEC_ID_H265,
  AV_CODEC_ID_FIC,
  AV_CODEC_ID_ALIAS_PIX,
  AV_CODEC_ID_BRENDER_PIX,
  AV_CODEC_ID_PAF_VIDEO,
  AV_CODEC_ID_EXR,
  AV_CODEC_ID_VP7,
  AV_CODEC_ID_SANM,
  AV_CODEC_ID_SGIRLE,
  AV_CODEC_ID_MVC1,
  AV_CODEC_ID_MVC2,
  AV_CODEC_ID_HQX,
  AV_CODEC_ID_TDSC,
  AV_CODEC_ID_HQ_HQA,
  AV_CODEC_ID_HAP,
  AV_CODEC_ID_DDS,
  AV_CODEC_ID_DXV,
  AV_CODEC_ID_SCREENPRESSO,
  AV_CODEC_ID_RSCC,
  AV_CODEC_ID_Y41P,
  AV_CODEC_ID_AVRP,
  AV_CODEC_ID_012V,
  AV_CODEC_ID_AVUI,
  AV_CODEC_ID_AYUV,
  AV_CODEC_ID_TARGA_Y216,
  AV_CODEC_ID_V308,
  AV_CODEC_ID_V408,
  AV_CODEC_ID_YUV4,
  AV_CODEC_ID_AVRN,
  AV_CODEC_ID_CPIA,
  AV_CODEC_ID_XFACE,
  AV_CODEC_ID_SNOW,
  AV_CODEC_ID_SMVJPEG,
  AV_CODEC_ID_APNG,
  AV_CODEC_ID_DAALA,
  AV_CODEC_ID_CFHD,
  AV_CODEC_ID_TRUEMOTION2RT,
  AV_CODEC_ID_M101,
  AV_CODEC_ID_MAGICYUV,
  AV_CODEC_ID_SHEERVIDEO,
  AV_CODEC_ID_YLC
  */
};
#define VIDEO_CODEC_IDS_LEN (sizeof(VIDEO_CODEC_IDS) / sizeof(enum AVCodecID))

enum AVCodecID VideoCodecId_val(value v)
{
  return VIDEO_CODEC_IDS[Int_val(v)];
}

value Val_videoCodecId(enum AVCodecID id)
{
  int i;
  for (i = 0; i < VIDEO_CODEC_IDS_LEN; i++) {
    if (id == VIDEO_CODEC_IDS[i])
      return Val_int(i);
  }
  Raise(EXN_FAILURE, "Invalid video codec id : %d\n", id);
  return Val_int(0);
}
/*
CAMLprim value ocaml_avcodec_get_video_codec_name(value _codec_id)
{
  CAMLparam1(_codec_id);
  CAMLreturn(caml_copy_string(avcodec_get_name(VideoCodecId_val(_codec_id))));
}
*/
CAMLprim value ocaml_avcodec_find_video_codec_id_by_name(value _name)
{
  CAMLparam1(_name);
  CAMLreturn(Val_videoCodecId(find_codec_id_by_name(String_val(_name))));
}


/**** Subtitle codec ID ****/
static const enum AVCodecID SUBTITLE_CODEC_IDS[] = {
  AV_CODEC_ID_DVD_SUBTITLE,
  AV_CODEC_ID_DVB_SUBTITLE,
  AV_CODEC_ID_TEXT,
  AV_CODEC_ID_XSUB,
  AV_CODEC_ID_SSA,
  AV_CODEC_ID_MOV_TEXT,
  AV_CODEC_ID_HDMV_PGS_SUBTITLE,
  AV_CODEC_ID_DVB_TELETEXT,
  AV_CODEC_ID_SRT,
  AV_CODEC_ID_MICRODVD,
  AV_CODEC_ID_EIA_608,
  AV_CODEC_ID_JACOSUB,
  AV_CODEC_ID_SAMI,
  AV_CODEC_ID_REALTEXT,
  AV_CODEC_ID_STL,
  AV_CODEC_ID_SUBVIEWER1,
  AV_CODEC_ID_SUBVIEWER,
  AV_CODEC_ID_SUBRIP,
  AV_CODEC_ID_WEBVTT,
  AV_CODEC_ID_MPL2,
  AV_CODEC_ID_VPLAYER,
  AV_CODEC_ID_PJS,
  AV_CODEC_ID_ASS,
  AV_CODEC_ID_HDMV_TEXT_SUBTITLE
};
#define SUBTITLE_CODEC_IDS_LEN (sizeof(SUBTITLE_CODEC_IDS) / sizeof(enum AVCodecID))

enum AVCodecID SubtitleCodecId_val(value v)
{
  return SUBTITLE_CODEC_IDS[Int_val(v)];
}

value Val_subtitleCodecId(enum AVCodecID id)
{
  int i;
  for (i = 0; i < SUBTITLE_CODEC_IDS_LEN; i++) {
    if (id == SUBTITLE_CODEC_IDS[i])
      return Val_int(i);
  }
  Raise(EXN_FAILURE, "Invalid subtitle codec id : %d\n", id);
  return Val_int(0);
}
/*
CAMLprim value ocaml_avcodec_get_subtitle_codec_name(value _codec_id)
{
  CAMLparam1(_codec_id);
  CAMLreturn(caml_copy_string(avcodec_get_name(SubtitleCodecId_val(_codec_id))));
}
*/
CAMLprim value ocaml_avcodec_find_subtitle_codec_id_by_name(value _name)
{
  CAMLparam1(_name);
  CAMLreturn(Val_subtitleCodecId(find_codec_id_by_name(String_val(_name))));
}

