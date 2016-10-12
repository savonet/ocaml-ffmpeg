open Avutil

module SF = Sample_format

type channel_layout = Channel_layout.t
type sample_format = Sample_format.t
type audio_frame = Audio_frame.t

type u8ba = (int, Bigarray.int8_unsigned_elt, Bigarray.c_layout) Bigarray.Array1.t
type s16ba = (int, Bigarray.int16_signed_elt, Bigarray.c_layout) Bigarray.Array1.t
type s32ba = (int32, Bigarray.int32_elt, Bigarray.c_layout) Bigarray.Array1.t
type f32ba = (float, Bigarray.float32_elt, Bigarray.c_layout) Bigarray.Array1.t
type f64ba = (float, Bigarray.float64_elt, Bigarray.c_layout) Bigarray.Array1.t

type ('i, 'o) converter

type vector_kind = Str | P_Str | Fa | P_Fa | Ba | P_Ba | Frm

module type AUDIO_DATA = sig
  type t
  val vk : vector_kind
  val sf : Sample_format.t option
end

module BTS = struct type t = bytes let vk = Str let sf = None end
module U8_BTS = struct type t = bytes let vk = Str let sf = Some SF.SF_U8 end
module S16_BTS = struct type t = bytes let vk = Str let sf = Some SF.SF_S16 end
module S32_BTS = struct type t = bytes let vk = Str let sf = Some SF.SF_S32 end
module FLT_BTS = struct type t = bytes let vk = Str let sf = Some SF.SF_FLT end
module DBL_BTS = struct type t = bytes let vk = Str let sf = Some SF.SF_DBL end
module U8P_BTS = struct type t = bytes array let vk = P_Str let sf = Some SF.SF_U8P end
module S16P_BTS = struct type t = bytes array let vk = P_Str let sf = Some SF.SF_S16P end
module S32P_BTS = struct type t = bytes array let vk = P_Str let sf = Some SF.SF_S32P end
module FLTP_BTS = struct type t = bytes array let vk = P_Str let sf = Some SF.SF_FLTP end
module DBLP_BTS = struct type t = bytes array let vk = P_Str let sf = Some SF.SF_DBLP end

module DBL_FA = struct type t = float array let vk = Fa let sf = Some SF.SF_DBL end
module DBLP_FA = struct type t = float array array let vk = P_Fa let sf = Some SF.SF_DBLP end

module U8_BA = struct type t = u8ba let vk = Ba let sf = Some SF.SF_U8 end
module S16_BA = struct type t = s16ba let vk = Ba let sf = Some SF.SF_S16 end
module S32_BA = struct type t = s32ba let vk = Ba let sf = Some SF.SF_S32 end
module FLT_BA = struct type t = f32ba let vk = Ba let sf = Some SF.SF_FLT end
module DBL_BA = struct type t = f64ba let vk = Ba let sf = Some SF.SF_DBL end
module U8P_BA = struct type t = u8ba array let vk = P_Ba let sf = Some SF.SF_U8P end
module S16P_BA = struct type t = s16ba array let vk = P_Ba let sf = Some SF.SF_S16P end
module S32P_BA = struct type t = s32ba array let vk = P_Ba let sf = Some SF.SF_S32P end
module FLTP_BA = struct type t = f32ba array let vk = P_Ba let sf = Some SF.SF_FLTP end
module DBLP_BA = struct type t = f64ba array let vk = P_Ba let sf = Some SF.SF_DBLP end

module FRM = struct type t = audio_frame let vk = Frm let sf = None end
module U8_FRM = struct type t = audio_frame let vk = Frm let sf = Some SF.SF_U8 end
module S16_FRM = struct type t = audio_frame let vk = Frm let sf = Some SF.SF_S16 end
module S32_FRM = struct type t = audio_frame let vk = Frm let sf = Some SF.SF_S32 end
module FLT_FRM = struct type t = audio_frame let vk = Frm let sf = Some SF.SF_FLT end
module DBL_FRM = struct type t = audio_frame let vk = Frm let sf = Some SF.SF_DBL end
module U8P_FRM = struct type t = audio_frame let vk = Frm let sf = Some SF.SF_U8P end
module S16P_FRM = struct type t = audio_frame let vk = Frm let sf = Some SF.SF_S16P end
module S32P_FRM = struct type t = audio_frame let vk = Frm let sf = Some SF.SF_S32P end
module FLTP_FRM = struct type t = audio_frame let vk = Frm let sf = Some SF.SF_FLTP end
module DBLP_FRM = struct type t = audio_frame let vk = Frm let sf = Some SF.SF_DBLP end


module Converter (I : AUDIO_DATA) (O : AUDIO_DATA) = struct
  type t = (I.t, O.t) converter

  external create : vector_kind -> channel_layout -> sample_format -> int ->
    vector_kind -> channel_layout -> sample_format -> int ->
    t = "ocaml_swresample_create_byte" "ocaml_swresample_create"

  let create in_channel_layout ?in_sample_format in_sample_rate
      out_channel_layout ?out_sample_format out_sample_rate =

    let in_sample_format = match (I.sf, in_sample_format) with
      | (Some sf, _) -> sf
      | (None, Some sf) -> sf
      | (None, None) -> raise(Failure_msg "Swresample.Converter input sample format undefined")
    in
    
    let out_sample_format = match (O.sf, out_sample_format) with
      | (Some sf, _) -> sf
      | (None, Some sf) -> sf
      | (None, None) -> raise(Failure_msg "Swresample.Converter output sample format undefined")
    in
    
    create I.vk in_channel_layout in_sample_format in_sample_rate
      O.vk out_channel_layout out_sample_format out_sample_rate

  
  external convert : t -> I.t -> O.t = "ocaml_swresample_convert"
end


module R = Converter (FRM) (S32_BA)


