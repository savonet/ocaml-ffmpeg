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

type t

external create : channel_layout -> sample_format -> int -> channel_layout -> sample_format -> int -> t = "ocaml_swresample_create_byte" "ocaml_swresample_create"

external frame_to_string : t -> audio_frame -> string = "ocaml_swresample_to_string"
external frame_to_planar_string : t -> audio_frame -> string array = "ocaml_swresample_to_planar_string"

external frame_to_float_array : t -> audio_frame -> float array = "ocaml_swresample_to_float_array"
external frame_to_float_planar_array : t -> audio_frame -> float array array = "ocaml_swresample_to_float_planar_array"


type ('i, 'o) converter

type vector_kind = Str | P_Str | Fa | P_Fa | Ba | P_Ba | Frm

module type AUDIO_DATA = sig
  type t
  val v : vector_kind
  val sf : Sample_format.t
end

module U8_STR = struct type t = string let v = Str let sf = SF.SF_U8 end
module S16_STR = struct type t = string let v = Str let sf = SF.SF_S16 end
module S32_STR = struct type t = string let v = Str let sf = SF.SF_S32 end
module FLT_STR = struct type t = string let v = Str let sf = SF.SF_FLT end
module DBL_STR = struct type t = string let v = Str let sf = SF.SF_DBL end
module U8P_STR = struct type t = string array let v = P_Str let sf = SF.SF_U8P end
module S16P_STR = struct type t = string array let v = P_Str let sf = SF.SF_S16P end
module S32P_STR = struct type t = string array let v = P_Str let sf = SF.SF_S32P end
module FLTP_STR = struct type t = string array let v = P_Str let sf = SF.SF_FLTP end
module DBLP_STR = struct type t = string array let v = P_Str let sf = SF.SF_DBLP end

module DBL_FA = struct type t = float array let v = Fa let sf = SF.SF_DBL end
module DBLP_FA = struct type t = float array array let v = P_Fa let sf = SF.SF_DBLP end

module U8_BA = struct type t = u8ba let v = Ba let sf = SF.SF_U8 end
module S16_BA = struct type t = s16ba let v = Ba let sf = SF.SF_S16 end
module S32_BA = struct type t = s32ba let v = Ba let sf = SF.SF_S32 end
module FLT_BA = struct type t = f32ba let v = Ba let sf = SF.SF_FLT end
module DBL_BA = struct type t = f64ba let v = Ba let sf = SF.SF_DBL end
module U8P_BA = struct type t = u8ba array let v = P_Ba let sf = SF.SF_U8P end
module S16P_BA = struct type t = s16ba array let v = P_Ba let sf = SF.SF_S16P end
module S32P_BA = struct type t = s32ba array let v = P_Ba let sf = SF.SF_S32P end
module FLTP_BA = struct type t = f32ba array let v = P_Ba let sf = SF.SF_FLTP end
module DBLP_BA = struct type t = f64ba array let v = P_Ba let sf = SF.SF_DBLP end

module U8_FRM = struct type t = audio_frame let v = Frm let sf = SF.SF_U8 end
module S16_FRM = struct type t = audio_frame let v = Frm let sf = SF.SF_S16 end
module S32_FRM = struct type t = audio_frame let v = Frm let sf = SF.SF_S32 end
module FLT_FRM = struct type t = audio_frame let v = Frm let sf = SF.SF_FLT end
module DBL_FRM = struct type t = audio_frame let v = Frm let sf = SF.SF_DBL end
module U8P_FRM = struct type t = audio_frame let v = Frm let sf = SF.SF_U8P end
module S16P_FRM = struct type t = audio_frame let v = Frm let sf = SF.SF_S16P end
module S32P_FRM = struct type t = audio_frame let v = Frm let sf = SF.SF_S32P end
module FLTP_FRM = struct type t = audio_frame let v = Frm let sf = SF.SF_FLTP end
module DBLP_FRM = struct type t = audio_frame let v = Frm let sf = SF.SF_DBLP end


module Converter (I : AUDIO_DATA) (O : AUDIO_DATA) = struct
  type t = (I.t, O.t) converter

  external create : vector_kind -> channel_layout -> sample_format -> int ->
    vector_kind -> channel_layout -> sample_format -> int ->
    t = "ocaml_swresample_create_byte" "ocaml_swresample_create"

  let create in_channel_layout in_sample_rate out_channel_layout out_sample_rate =
    create I.v in_channel_layout I.sf in_sample_rate O.v out_channel_layout O.sf out_sample_rate

  external convert : t -> I.t -> O.t = "ocaml_swresample_convert"
end

(*
module FrameToBigarray (Output : AUDIO_DATA) = struct
  type t = (audio_frame, Output.t)converter

  external create : channel_layout -> sample_format -> int -> channel_layout -> sample_format -> int -> t = "ocaml_swresample_create_byte" "ocaml_swresample_create"

  let create in_channel_layout in_sample_format in_sample_rate out_channel_layout out_sample_rate =
    create in_channel_layout in_sample_format in_sample_rate out_channel_layout Output.sample_format out_sample_rate

  external convert : t -> audio_frame -> Output.t = "ocaml_swresample_frame_to_ba"
end

module BigarrayToBigarray (Input : AUDIO_DATA) (Output : AUDIO_DATA) = struct
  type t = (Input.t, Output.t) converter

  external create : channel_layout -> sample_format -> int -> channel_layout -> sample_format -> int -> t = "ocaml_swresample_create_byte" "ocaml_swresample_create"

  let create in_channel_layout in_sample_rate out_channel_layout out_sample_rate =
    create in_channel_layout Input.sf in_sample_rate out_channel_layout Output.sample_format out_sample_rate

  external convert : t -> Input.t -> Output.t = "ocaml_swresample_ba_to_ba"
end
*)


