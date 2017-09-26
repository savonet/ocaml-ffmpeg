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

type vector_kind = Str | P_Str | Fa | P_Fa | Ba | P_Ba | Frm

module type AudioData = sig
  type t
  val vk : vector_kind
  val sf : Sample_format.t
end

module Bytes = struct
  type t = bytes let vk = Str let sf = SF.SF_none
end

module U8Bytes = struct
  type t = bytes let vk = Str let sf = SF.SF_U8
end

module S16Bytes = struct
  type t = bytes let vk = Str let sf = SF.SF_S16
end

module S32Bytes = struct
  type t = bytes let vk = Str let sf = SF.SF_S32
end

module FltBytes = struct
  type t = bytes let vk = Str let sf = SF.SF_FLT
end

module DblBytes = struct
  type t = bytes let vk = Str let sf = SF.SF_DBL
end

module U8PlanarBytes = struct
  type t = bytes array let vk = P_Str let sf = SF.SF_U8P
end

module S16PlanarBytes = struct
  type t = bytes array let vk = P_Str let sf = SF.SF_S16P
end

module S32PlanarBytes = struct
  type t = bytes array let vk = P_Str let sf = SF.SF_S32P
end

module FltPlanarBytes = struct
  type t = bytes array let vk = P_Str let sf = SF.SF_FLTP
end

module DblPlanarBytes = struct
  type t = bytes array let vk = P_Str let sf = SF.SF_DBLP
end

module FloatArray = struct
  type t = float array let vk = Fa let sf = SF.SF_DBL
end

module PlanarFloatArray = struct
  type t = float array array let vk = P_Fa let sf = SF.SF_DBLP
end

module U8BigArray = struct
  type t = u8ba let vk = Ba let sf = SF.SF_U8
end

module S16BigArray = struct
  type t = s16ba let vk = Ba let sf = SF.SF_S16
end

module S32BigArray = struct
  type t = s32ba let vk = Ba let sf = SF.SF_S32
end

module FltBigArray = struct
  type t = f32ba let vk = Ba let sf = SF.SF_FLT
end

module DblBigArray = struct
  type t = f64ba let vk = Ba let sf = SF.SF_DBL
end

module U8PlanarBigArray = struct
  type t = u8ba array let vk = P_Ba let sf = SF.SF_U8P
end

module S16PlanarBigArray = struct
  type t = s16ba array let vk = P_Ba let sf = SF.SF_S16P
end

module S32PlanarBigArray = struct
  type t = s32ba array let vk = P_Ba let sf = SF.SF_S32P
end

module FltPlanarBigArray = struct
  type t = f32ba array let vk = P_Ba let sf = SF.SF_FLTP
end

module DblPlanarBigArray = struct
  type t = f64ba array let vk = P_Ba let sf = SF.SF_DBLP
end

module Frame = struct
  type t = audio_frame let vk = Frm let sf = SF.SF_none
end

module U8Frame = struct
  type t = audio_frame let vk = Frm let sf = SF.SF_U8
end

module S16Frame = struct
  type t = audio_frame let vk = Frm let sf = SF.SF_S16
end

module S32Frame = struct
  type t = audio_frame let vk = Frm let sf = SF.SF_S32
end

module FltFrame = struct
  type t = audio_frame let vk = Frm let sf = SF.SF_FLT
end

module DblFrame = struct
  type t = audio_frame let vk = Frm let sf = SF.SF_DBL
end

module U8PlanarFrame = struct
  type t = audio_frame let vk = Frm let sf = SF.SF_U8P
end

module S16PlanarFrame = struct
  type t = audio_frame let vk = Frm let sf = SF.SF_S16P
end

module S32PlanarFrame = struct
  type t = audio_frame let vk = Frm let sf = SF.SF_S32P
end

module FltPlanarFrame = struct
  type t = audio_frame let vk = Frm let sf = SF.SF_FLTP
end

module DblPlanarFrame = struct
  type t = audio_frame let vk = Frm let sf = SF.SF_DBLP
end


type ('i, 'o) t

module Make (I : AudioData) (O : AudioData) = struct
  type nonrec t = (I.t, O.t) t

  external create : vector_kind -> channel_layout -> sample_format -> int ->
    vector_kind -> channel_layout -> sample_format -> int ->
    t = "ocaml_swresample_create_byte" "ocaml_swresample_create"


  let create in_channel_layout ?in_sample_format in_sample_rate
      out_channel_layout ?out_sample_format out_sample_rate =

    let in_sample_format = match in_sample_format with
      | _ when I.sf <> SF.SF_none -> I.sf
      | Some sf -> sf
      | _ -> raise(Failure "Swresample.Converter input sample format undefined")
    in
    let out_sample_format = match out_sample_format with
      | _ when O.sf <> SF.SF_none -> O.sf
      | Some sf -> sf
      | _ -> raise(Failure "Swresample.Converter output sample format undefined")
    in
    create I.vk in_channel_layout in_sample_format in_sample_rate
      O.vk out_channel_layout out_sample_format out_sample_rate


  let from_codec_parameters in_codec_param out_channel_layout ?out_sample_format out_sample_rate =

    create (Avcodec.Audio.Parameters.get_channel_layout in_codec_param)
      ~in_sample_format:(Avcodec.Audio.Parameters.get_sample_format in_codec_param)
      (Avcodec.Audio.Parameters.get_sample_rate in_codec_param)
      out_channel_layout
      ?out_sample_format:out_sample_format
      out_sample_rate


  let to_codec_parameters in_channel_layout ?in_sample_format in_sample_rate out_codec_param =

    create in_channel_layout ?in_sample_format:in_sample_format in_sample_rate
      (Avcodec.Audio.Parameters.get_channel_layout out_codec_param)
      ~out_sample_format:(Avcodec.Audio.Parameters.get_sample_format out_codec_param)
      (Avcodec.Audio.Parameters.get_sample_rate out_codec_param)


  let from_codec_parameters_to_codec_parameters in_codec_param out_codec_param =

    create (Avcodec.Audio.Parameters.get_channel_layout in_codec_param)
      ~in_sample_format:(Avcodec.Audio.Parameters.get_sample_format in_codec_param)
      (Avcodec.Audio.Parameters.get_sample_rate in_codec_param)
      (Avcodec.Audio.Parameters.get_channel_layout out_codec_param)
      ~out_sample_format:(Avcodec.Audio.Parameters.get_sample_format out_codec_param)
      (Avcodec.Audio.Parameters.get_sample_rate out_codec_param)


  let from_input input out_channel_layout ?out_sample_format out_sample_rate =
    (*    let ip = Av.(get_audio_codec_parameters(Input.base input)) in*)
    let ip = Av.(input >- get_audio_codec_parameters) in
    from_codec_parameters ip out_channel_layout ?out_sample_format out_sample_rate


  let to_output in_channel_layout ?in_sample_format in_sample_rate output =
    let op = Av.(get_audio_codec_parameters(Output.base output)) in
    to_codec_parameters in_channel_layout ?in_sample_format in_sample_rate op


  let from_input_to_output input output =
(*    let ip = Av.(get_audio_codec_parameters(Input.base input)) in*)
    let ip = Av.(get_audio_codec_parameters @@ of_input input) in
    let op = Av.(get_audio_codec_parameters(Output.base output)) in
    from_codec_parameters_to_codec_parameters ip op


  external convert : t -> I.t -> O.t = "ocaml_swresample_convert"
end
