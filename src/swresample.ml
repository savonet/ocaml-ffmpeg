open Avutil

module SF = Avutil.Sample_format
module CL = Avutil.Channel_layout

type u8ba = (int, Bigarray.int8_unsigned_elt, Bigarray.c_layout) Bigarray.Array1.t
type s16ba = (int, Bigarray.int16_signed_elt, Bigarray.c_layout) Bigarray.Array1.t
type s32ba = (int32, Bigarray.int32_elt, Bigarray.c_layout) Bigarray.Array1.t
type f32ba = (float, Bigarray.float32_elt, Bigarray.c_layout) Bigarray.Array1.t
type f64ba = (float, Bigarray.float64_elt, Bigarray.c_layout) Bigarray.Array1.t

type vector_kind = Str | P_Str | Fa | P_Fa | Ba | P_Ba | Frm

module type AudioData = sig
  type t
  val vk : vector_kind
  val sf : SF.t
end

module Bytes = struct
  type t = bytes let vk = Str let sf = `NONE
end

module U8Bytes = struct
  type t = bytes let vk = Str let sf = `U8
end

module S16Bytes = struct
  type t = bytes let vk = Str let sf = `S16
end

module S32Bytes = struct
  type t = bytes let vk = Str let sf = `S32
end

module FltBytes = struct
  type t = bytes let vk = Str let sf = `FLT
end

module DblBytes = struct
  type t = bytes let vk = Str let sf = `DBL
end

module U8PlanarBytes = struct
  type t = bytes array let vk = P_Str let sf = `U8P
end

module S16PlanarBytes = struct
  type t = bytes array let vk = P_Str let sf = `S16P
end

module S32PlanarBytes = struct
  type t = bytes array let vk = P_Str let sf = `S32P
end

module FltPlanarBytes = struct
  type t = bytes array let vk = P_Str let sf = `FLTP
end

module DblPlanarBytes = struct
  type t = bytes array let vk = P_Str let sf = `DBLP
end

module FloatArray = struct
  type t = float array let vk = Fa let sf = `DBL
end

module PlanarFloatArray = struct
  type t = float array array let vk = P_Fa let sf = `DBLP
end

module U8BigArray = struct
  type t = u8ba let vk = Ba let sf = `U8
end

module S16BigArray = struct
  type t = s16ba let vk = Ba let sf = `S16
end

module S32BigArray = struct
  type t = s32ba let vk = Ba let sf = `S32
end

module FltBigArray = struct
  type t = f32ba let vk = Ba let sf = `FLT
end

module DblBigArray = struct
  type t = f64ba let vk = Ba let sf = `DBL
end

module U8PlanarBigArray = struct
  type t = u8ba array let vk = P_Ba let sf = `U8P
end

module S16PlanarBigArray = struct
  type t = s16ba array let vk = P_Ba let sf = `S16P
end

module S32PlanarBigArray = struct
  type t = s32ba array let vk = P_Ba let sf = `S32P
end

module FltPlanarBigArray = struct
  type t = f32ba array let vk = P_Ba let sf = `FLTP
end

module DblPlanarBigArray = struct
  type t = f64ba array let vk = P_Ba let sf = `DBLP
end

module Frame = struct
  type t = audio frame let vk = Frm let sf = `NONE
end

module U8Frame = struct
  type t = audio frame let vk = Frm let sf = `U8
end

module S16Frame = struct
  type t = audio frame let vk = Frm let sf = `S16
end

module S32Frame = struct
  type t = audio frame let vk = Frm let sf = `S32
end

module FltFrame = struct
  type t = audio frame let vk = Frm let sf = `FLT
end

module DblFrame = struct
  type t = audio frame let vk = Frm let sf = `DBL
end

module U8PlanarFrame = struct
  type t = audio frame let vk = Frm let sf = `U8P
end

module S16PlanarFrame = struct
  type t = audio frame let vk = Frm let sf = `S16P
end

module S32PlanarFrame = struct
  type t = audio frame let vk = Frm let sf = `S32P
end

module FltPlanarFrame = struct
  type t = audio frame let vk = Frm let sf = `FLTP
end

module DblPlanarFrame = struct
  type t = audio frame let vk = Frm let sf = `DBLP
end


type ('i, 'o) ctx

module Make (I : AudioData) (O : AudioData) = struct
  type t = (I.t, O.t) ctx

  external create : vector_kind -> CL.t -> SF.t -> int ->
    vector_kind -> CL.t -> SF.t -> int ->
    t = "ocaml_swresample_create_byte" "ocaml_swresample_create"


  let create in_channel_layout ?in_sample_format in_sample_rate
        out_channel_layout ?out_sample_format out_sample_rate =
      
    let in_sample_format = match in_sample_format with
      | _ when I.sf <> `NONE -> I.sf
      | Some sf -> sf
      | _ -> raise(Failure "Swresample input sample format undefined")
    in
    let out_sample_format = match out_sample_format with
      | _ when O.sf <> `NONE -> O.sf
      | Some sf -> sf
      | _ -> raise(Failure "Swresample output sample format undefined")
    in
    create I.vk in_channel_layout in_sample_format in_sample_rate
      O.vk out_channel_layout out_sample_format out_sample_rate


  let from_codec in_codec out_channel_layout ?out_sample_format out_sample_rate =

    create (Avcodec.Audio.get_channel_layout in_codec)
      ~in_sample_format:(Avcodec.Audio.get_sample_format in_codec)
      (Avcodec.Audio.get_sample_rate in_codec)
      out_channel_layout
      ?out_sample_format:out_sample_format
      out_sample_rate


  let to_codec in_channel_layout ?in_sample_format in_sample_rate out_codec =

    create in_channel_layout ?in_sample_format:in_sample_format in_sample_rate
      (Avcodec.Audio.get_channel_layout out_codec)
      ~out_sample_format:(Avcodec.Audio.get_sample_format out_codec)
      (Avcodec.Audio.get_sample_rate out_codec)


  let from_codec_to_codec in_codec out_codec =

    create (Avcodec.Audio.get_channel_layout in_codec)
      ~in_sample_format:(Avcodec.Audio.get_sample_format in_codec)
      (Avcodec.Audio.get_sample_rate in_codec)
      (Avcodec.Audio.get_channel_layout out_codec)
      ~out_sample_format:(Avcodec.Audio.get_sample_format out_codec)
      (Avcodec.Audio.get_sample_rate out_codec)


  external reuse_output : t -> bool -> unit = "ocaml_swresample_reuse_output"

  external convert : t -> I.t -> O.t = "ocaml_swresample_convert"
end
