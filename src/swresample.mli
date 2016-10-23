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

module type AudioData =
  sig type t val vk : vector_kind val sf : Avutil.Sample_format.t option end

module Bytes : sig type t = bytes val vk : vector_kind val sf : 'a option end

module U8Bytes :
  sig type t = bytes val vk : vector_kind val sf : SF.t option end

module S16Bytes :
  sig type t = bytes val vk : vector_kind val sf : SF.t option end

module S32Bytes :
  sig type t = bytes val vk : vector_kind val sf : SF.t option end

module FltBytes :
  sig type t = bytes val vk : vector_kind val sf : SF.t option end

module DblBytes :
  sig type t = bytes val vk : vector_kind val sf : SF.t option end

module U8PlanarBytes :
  sig type t = bytes array val vk : vector_kind val sf : SF.t option end

module S16PlanarBytes :
  sig type t = bytes array val vk : vector_kind val sf : SF.t option end

module S32PlanarBytes :
  sig type t = bytes array val vk : vector_kind val sf : SF.t option end

module FltPlanarBytes :
  sig type t = bytes array val vk : vector_kind val sf : SF.t option end

module DblPlanarBytes :
  sig type t = bytes array val vk : vector_kind val sf : SF.t option end

module FloatArray :
  sig type t = float array val vk : vector_kind val sf : SF.t option end

module PlanarFloatArray :
  sig type t = float array array val vk : vector_kind val sf : SF.t option end

module U8BigArray :
  sig type t = u8ba val vk : vector_kind val sf : SF.t option end

module S16BigArray :
  sig type t = s16ba val vk : vector_kind val sf : SF.t option end

module S32BigArray :
  sig type t = s32ba val vk : vector_kind val sf : SF.t option end

module FltBigArray :
  sig type t = f32ba val vk : vector_kind val sf : SF.t option end

module DblBigArray :
  sig type t = f64ba val vk : vector_kind val sf : SF.t option end

module U8PlanarBigArray :
  sig type t = u8ba array val vk : vector_kind val sf : SF.t option end

module S16PlanarBigArray :
  sig type t = s16ba array val vk : vector_kind val sf : SF.t option end

module S32PlanarBigArray :
  sig type t = s32ba array val vk : vector_kind val sf : SF.t option end

module FltPlanarBigArray :
  sig type t = f32ba array val vk : vector_kind val sf : SF.t option end

module DblPlanarBigArray :
  sig type t = f64ba array val vk : vector_kind val sf : SF.t option end

module Frame :
  sig type t = audio_frame val vk : vector_kind val sf : 'a option end

module U8Frame :
  sig type t = audio_frame val vk : vector_kind val sf : SF.t option end

module S16Frame :
  sig type t = audio_frame val vk : vector_kind val sf : SF.t option end

module S32Frame :
  sig type t = audio_frame val vk : vector_kind val sf : SF.t option end

module FltFrame :
  sig type t = audio_frame val vk : vector_kind val sf : SF.t option end

module DblFrame :
  sig type t = audio_frame val vk : vector_kind val sf : SF.t option end

module U8PlanarFrame :
  sig type t = audio_frame val vk : vector_kind val sf : SF.t option end

module S16PlanarFrame :
  sig type t = audio_frame val vk : vector_kind val sf : SF.t option end

module S32PlanarFrame :
  sig type t = audio_frame val vk : vector_kind val sf : SF.t option end

module FltPlanarFrame :
  sig type t = audio_frame val vk : vector_kind val sf : SF.t option end

module DblPlanarFrame :
  sig type t = audio_frame val vk : vector_kind val sf : SF.t option end

type ('i, 'o) t

module Make :
  functor (I : AudioData) (O : AudioData) ->
    sig
      type nonrec t = (I.t, O.t) t

      val create :
        channel_layout ->
        ?in_sample_format:sample_format ->
        int -> channel_layout -> ?out_sample_format:sample_format -> int -> t

      val of_in_audio_format :
        Avutil.audio_format ->
        channel_layout -> ?out_sample_format:sample_format -> int -> t

      val of_audio_formats : Avutil.audio_format -> Avutil.audio_format -> t

      val convert : t -> I.t -> O.t
    end
