(** Audio, video and subtitle encoder and decoder *)

open Avutil

type 'a t
type ('line, 'media) context
type use = [ `decoder | `encoder]

(* type 'media result = [ `frame of 'media frame | `end_of_stream | `end_of_stream ] *)


(** Audio codecs. *)
module Audio : sig
  (** Audio codec ids *)
  type id = Codec_id.audio

  (** Return the name of the codec. *)
  val get_name : id -> string

  val find_id : string -> id
  (** Return the id of a codec from his name.
      @raise Failure if the codec is not found or is not an audio codec. *)

  val find_best_sample_format : id -> Avutil.Sample_format.t
  (** Return the best sample format of a codec.
      @raise Failure if the codec has no best sample format. *)

  (** Return the id of the codec. *)
  val get_id : audio t -> id

  (** Return the channel layout set for the codec. *)
  val get_channel_layout : audio t -> Avutil.Channel_layout.t

  (** Returns the number of channels set for the codec. *)
  val get_nb_channels : audio t -> int

  (** Returns the sample format set for the codec. *)
  val get_sample_format : audio t -> Avutil.Sample_format.t

  (** Returns the bit rate set for the codec. *)
  val get_bit_rate : audio t -> int

  (** Returns the sample rate set for the codec. *)
  val get_sample_rate : audio t -> int

  val create_decoder_context : id -> (input, audio)context

  val create_encoder_context : ?bit_rate:int -> id -> (output, audio)context

  val decode : (input, audio)context -> bytes -> int -> audio frame array
  val decode_data : (input, audio)context -> data -> int -> audio frame array

  val encode : (output, audio)context -> audio frame -> bytes
  val encode_to_data : (output, audio)context -> audio frame -> data
end

(** Video codecs. *)
module Video : sig
  (** Video codec ids *)
  type id = Codec_id.video

  (** Return the name of the codec. *)
  val get_name : id -> string

  val find_id : string -> id
  (** Return the id of a codec from his name.
      @raise Failure if the codec is not found or is not a video codec. *)

  val find_best_pixel_format : id -> Avutil.Pixel_format.t
  (** Return the best pixel format of a codec.
      @raise Failure if the codec has no best pixel format. *)

  (** Return the id of the codec. *)
  val get_id : video t -> id

  (** Returns the width set for the codec. *)
  val get_width : video t -> int

  (** Returns the height set for the codec. *)
  val get_height : video t -> int

  (** Returns the sample aspect ratio set for the codec. *)
  val get_sample_aspect_ratio : video t -> (int * int)

  (** Returns the pixel format set for the codec. *)
  val get_pixel_format : video t -> Avutil.Pixel_format.t

  (** Returns the bit rate set for the codec. *)
  val get_bit_rate : video t -> int

  val create_decoder_context : id -> (input, video)context
  val create_encoder_context : ?bit_rate:int -> ?frame_rate:int -> id -> (output, video)context
  
  val decode : (input, video)context -> bytes -> int -> video frame array
  val decode_data : (input, video)context -> data -> int -> video frame array

  val encode : (output, video)context -> video frame -> bytes
  val encode_to_data : (output, video)context -> video frame -> data
end

(** Subtitle codecs. *)
module Subtitle : sig
  (** Subtitle codec ids *)
  type id = Codec_id.subtitle

(** Return the name of the codec. *)
  val get_name : id -> string

  val find_id : string -> id
  (** Return the id of a codec from his name.
      @raise Failure if the codec is not found or is not a subtitle codec. *)

  (** Return the id of the codec. *)
  val get_id : subtitle t -> id
end
