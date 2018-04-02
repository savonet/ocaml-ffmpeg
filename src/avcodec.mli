(** Audio, video and subtitle encoder and decoder *)

open Avutil

type 'a t
type 'media decoder
type 'media encoder


(** Packet. *)
module Packet : sig
  (** Packet type *)
  type 'a t

  (** Parser type *)
  type 'a parser

  (** Return the size of the packet. *)
  val get_size : 'a t -> int

  (** Return the stream index of the packet. *)
  val get_stream_index : 'a t -> int

  (** Set the stream index of the packet. *)
  val set_stream_index : 'a t -> int -> unit

  (** Return a fresh bytes array containing a copy of packet datas. *)
  val to_bytes : 'a t -> bytes

  val parse_data : 'a parser -> ('a t -> unit) -> data -> unit
  (** [Avcodec.Packet.parse_data parser f data] applies function [f] to the parsed packets frome the [data] array according to the [parser] configuration.
    @raise Failure if the parsing failed. *)

  val parse_bytes : 'a parser -> ('a t -> unit) -> bytes -> int -> unit
  (** Same as {!Avcodec.Packet.parse_data} with bytes array. *)
end


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

  val create_parser : id -> audio Packet.parser
  (** [Avcodec.Audio.create_parser id] create an audio packet parser.
      @raise Failure if the parser creation failed. *)

  val create_decoder : id -> audio decoder
  (** [Avcodec.Audio.create_decoder id] create an audio decoder.
      @raise Failure if the decoder creation failed. *)

  val create_encoder : ?bit_rate:int -> id -> audio encoder
  (** [Avcodec.Audio.create_encoder ~bit_rate:bit_rate id] create an audio encoder.
      @raise Failure if the encoder creation failed. *)
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


  val create_parser : id -> video Packet.parser
  (** [Avcodec.Video.create_parser id] create an video packet parser.
      @raise Failure if the parser creation failed. *)

  val create_decoder : id -> video decoder
  (** [Avcodec.Video.create_decoder id] create a video decoder.
      @raise Failure if the decoder creation failed. *)

  val create_encoder : ?bit_rate:int -> ?frame_rate:int -> id -> video encoder
  (** [Avcodec.Video.create_encoder ~bit_rate:bit_rate id] create a video encoder.
      @raise Failure if the encoder creation failed. *)
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


val decode : 'media decoder -> ('media frame -> unit) -> 'media Packet.t -> unit
(** [Avcodec.decode decoder f packet] applies function [f] to the decoded frames frome the [packet] according to the [decoder] configuration.
    @raise Failure if the decoding failed. *)

val flush_decoder : 'media decoder -> ('media frame -> unit) -> unit
(** [Avcodec.flush_decoder decoder f] applies function [f] to the decoded frames frome the buffered packets in the [decoder].
    @raise Failure if the decoding failed. *)

val encode : 'media encoder -> ('media Packet.t -> unit) -> 'media frame -> unit
(** [Avcodec.encode encoder f frame] applies function [f] to the encoded packets from the [frame] according to the [encoder] configuration.
    @raise Failure if the encoding failed. *)

val flush_encoder : 'media encoder -> ('media Packet.t -> unit) -> unit
(** [Avcodec.flush_encoder encoder] applies function [f] to the encoded packets from the buffered frames in the [encoder].
    @raise Failure if the encoding failed. *)
