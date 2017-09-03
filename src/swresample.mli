module SF = Avutil.Sample_format

type channel_layout = Avutil.Channel_layout.t
type sample_format = Avutil.Sample_format.t
type audio_frame = Avutil.Audio_frame.t

type u8ba =
    (int, Bigarray.int8_unsigned_elt, Bigarray.c_layout) Bigarray.Array1.t
type s16ba =
    (int, Bigarray.int16_signed_elt, Bigarray.c_layout) Bigarray.Array1.t
type s32ba = (int32, Bigarray.int32_elt, Bigarray.c_layout) Bigarray.Array1.t
type f32ba =
    (float, Bigarray.float32_elt, Bigarray.c_layout) Bigarray.Array1.t
type f64ba =
    (float, Bigarray.float64_elt, Bigarray.c_layout) Bigarray.Array1.t

type vector_kind = Str | P_Str | Fa | P_Fa | Ba | P_Ba | Frm

module type AudioData = sig
  type t val vk : vector_kind val sf : Avutil.Sample_format.t
end

(** Audio data modules for Swresample module input and output parameterization. *)

(** Byte string with undefined sample format for interleaved channels. *)
module Bytes : sig
  type t = bytes val vk : vector_kind val sf : SF.t
end

(** Unsigned 8 bit sample format byte string for interleaved channels. *)
module U8Bytes : sig
  type t = bytes val vk : vector_kind val sf : SF.t
end

(** Signed 16 bit sample format byte string for interleaved channels. *)
module S16Bytes : sig
  type t = bytes val vk : vector_kind val sf : SF.t
end

(** Signed 32 bit sample format byte string for interleaved channels. *)
module S32Bytes : sig
  type t = bytes val vk : vector_kind val sf : SF.t
end

(** Float 32 bit sample format byte string for interleaved channels. *)
module FltBytes : sig
  type t = bytes val vk : vector_kind val sf : SF.t
end

(** Float 64 bit sample format byte string for interleaved channels. *)
module DblBytes : sig
  type t = bytes val vk : vector_kind val sf : SF.t
end

(** Unsigned 8 bit sample format byte string for planar channels. *)
module U8PlanarBytes : sig
  type t = bytes array val vk : vector_kind val sf : SF.t
end

(** Signed 16 bit sample format byte string for planar channels. *)
module S16PlanarBytes : sig
  type t = bytes array val vk : vector_kind val sf : SF.t
end

(** Signed 32 bit sample format byte string for planar channels. *)
module S32PlanarBytes : sig
  type t = bytes array val vk : vector_kind val sf : SF.t
end

(** Float 32 bit sample format byte string for planar channels. *)
module FltPlanarBytes : sig
  type t = bytes array val vk : vector_kind val sf : SF.t
end

(** Float 64 bit sample format byte string for planar channels. *)
module DblPlanarBytes : sig
  type t = bytes array val vk : vector_kind val sf : SF.t
end

(** Float 64 bit sample format array for interleaved channels. *)
module FloatArray : sig
  type t = float array val vk : vector_kind val sf : SF.t
end

(** Float 64 bit sample format array for planar channels. *)
module PlanarFloatArray : sig
  type t = float array array val vk : vector_kind val sf : SF.t
end

(** Unsigned 8 bit sample format bigarray for interleaved channels. *)
module U8BigArray : sig
  type t = u8ba val vk : vector_kind val sf : SF.t
end

(** Signed 16 bit sample format bigarray for interleaved channels. *)
module S16BigArray : sig
  type t = s16ba val vk : vector_kind val sf : SF.t
end

(** Signed 32 bit sample format bigarray for interleaved channels. *)
module S32BigArray : sig
  type t = s32ba val vk : vector_kind val sf : SF.t
end

(** Float 32 bit sample format bigarray for interleaved channels. *)
module FltBigArray : sig
  type t = f32ba val vk : vector_kind val sf : SF.t
end

(** Float 64 bit sample format bigarray for interleaved channels. *)
module DblBigArray : sig
  type t = f64ba val vk : vector_kind val sf : SF.t
end

(** Unsigned 8 bit sample format bigarray for planar channels. *)
module U8PlanarBigArray : sig
  type t = u8ba array val vk : vector_kind val sf : SF.t
end

(** Signed 16 bit sample format bigarray for planar channels. *)
module S16PlanarBigArray : sig
  type t = s16ba array val vk : vector_kind val sf : SF.t
end

(** Signed 32 bit sample format bigarray for planar channels. *)
module S32PlanarBigArray : sig
  type t = s32ba array val vk : vector_kind val sf : SF.t
end

(** Float 32 bit sample format bigarray for planar channels. *)
module FltPlanarBigArray : sig
  type t = f32ba array val vk : vector_kind val sf : SF.t
end

(** Float 64 bit sample format bigarray for planar channels. *)
module DblPlanarBigArray : sig
  type t = f64ba array val vk : vector_kind val sf : SF.t
end

(** Audio frame with undefined sample format. *)
module Frame : sig
  type t = audio_frame val vk : vector_kind val sf : SF.t
end

(** Unsigned 8 bit sample format audio frame for interleaved channels. *)
module U8Frame : sig
  type t = audio_frame val vk : vector_kind val sf : SF.t
end

(** Signed 16 bit sample format audio frame for interleaved channels. *)
module S16Frame : sig
  type t = audio_frame val vk : vector_kind val sf : SF.t
end

(** Signed 32 bit sample format audio frame for interleaved channels. *)
module S32Frame : sig
  type t = audio_frame val vk : vector_kind val sf : SF.t
end

(** Float 32 bit sample format audio frame for interleaved channels. *)
module FltFrame : sig
  type t = audio_frame val vk : vector_kind val sf : SF.t
end

(** Float 64 bit sample format audio frame for interleaved channels. *)
module DblFrame : sig
  type t = audio_frame val vk : vector_kind val sf : SF.t
end

(** Unsigned 8 bit sample format audio frame for planar channels. *)
module U8PlanarFrame : sig
  type t = audio_frame val vk : vector_kind val sf : SF.t
end

(** Signed 16 bit sample format audio frame for planar channels. *)
module S16PlanarFrame : sig
  type t = audio_frame val vk : vector_kind val sf : SF.t
end

(** Signed 32 bit sample format audio frame for planar channels. *)
module S32PlanarFrame : sig
  type t = audio_frame val vk : vector_kind val sf : SF.t
end

(** Float 32 bit sample format audio frame for planar channels. *)
module FltPlanarFrame : sig
  type t = audio_frame val vk : vector_kind val sf : SF.t
end

(** Float 64 bit sample format audio frame for planar channels. *)
module DblPlanarFrame : sig
  type t = audio_frame val vk : vector_kind val sf : SF.t
end

type ('i, 'o) t

(** Functor building an implementation of the swresample structure with parameterized input an output audio data types *)
module Make :
  functor (I : AudioData) (O : AudioData) ->
    sig
      type nonrec t = (I.t, O.t) t

      (** Create a Swresample.t with parameterized input and output audio format. *)
      val create : channel_layout -> ?in_sample_format:sample_format ->
        int -> channel_layout -> ?out_sample_format:sample_format -> int -> t

      (** Create a Swresample.t with audio_format properties input format and output audio format parameters. *)
      val from_audio_format : Avutil.audio_format ->
        channel_layout -> ?out_sample_format:sample_format -> int -> t

      (** Create a Swresample.t with input and output audio_format properties. *)
      val of_audio_formats : Avutil.audio_format -> Avutil.audio_format -> t

      (** Create a Swresample.t with parameterized input and output codec best audio format. *)
      val to_codec : channel_layout -> ?in_sample_format:sample_format ->
        int -> Avcodec.Audio.id -> channel_layout -> int -> t

      (** Create a Swresample.t with audio_format properties input format and output codec best audio format. *)
      val from_audio_format_to_codec : Avutil.audio_format ->
        Avcodec.Audio.id -> channel_layout -> int -> t

      (** Create a Swresample.t with input and output. *)
      val from_input_to_output : Av.input_t -> Av.output_t -> t

    (** Resample and convert input audio data to output audio data format. *)
      val convert : t -> I.t -> O.t
    end
