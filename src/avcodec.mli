(** This module contains decoders and encoders for audio, video and subtitle codecs. *)

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
    @raise Error if the parsing failed. *)

  val parse_bytes : 'a parser -> ('a t -> unit) -> bytes -> int -> unit
  (** Same as {!Avcodec.Packet.parse_data} with bytes array. *)

end


(** Audio codecs. *)
module Audio : sig
  (** Audio codec ids *)
  type id = Codec_id.audio

  (** Return the name of the codec. *)
  val get_name : id -> string

  val find_encoder_id : string -> id
  (** Return the id of a encoder codec from its name.
      @raise Error if the codec is not found or is not an audio codec. *)

  val find_decoder_id : string -> id
  (** Return the id of a encoder codec from its name.
      @raise Error if the codec is not found or is not an audio codec. *)

  (** Return the list of supported channel layouts of the codec. *)
  val get_supported_channel_layouts : id -> Avutil.Channel_layout.t list
      
  val find_best_channel_layout : id -> Avutil.Channel_layout.t -> Avutil.Channel_layout.t
  (** [Avcodec.Audio.find_best_channel_layout id default] return the best channel layout of the [id] codec or the [default] value if the codec has no channel layout. *)

  (** Return the list of supported sample formats of the codec. *)
  val get_supported_sample_formats : id -> Avutil.Sample_format.t list
      
  val find_best_sample_format : id -> Avutil.Sample_format.t -> Avutil.Sample_format.t
  (** [Avcodec.Audio.find_best_sample_format id default] return the best sample format of the [id] codec or the [default] value if the codec has no sample format. *)

  (** Return the list of supported sample rates of the codec. *)
  val get_supported_sample_rates : id -> int list
      
  val find_best_sample_rate : id -> int -> int
  (** [Avcodec.Audio.find_best_sample_rate id default] return the best sample rate of the [id] codec or the [default] value if the codec has no sample rate. *)

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
      @raise Error if the parser creation failed. *)

  val create_decoder : id -> audio decoder
  (** [Avcodec.Audio.create_decoder id] create an audio decoder.
      @raise Error if the decoder creation failed. *)

  val create_encoder : ?bit_rate:int -> id -> audio encoder
  (** [Avcodec.Audio.create_encoder ~bit_rate:bit_rate id] create an audio encoder.
      @raise Error if the encoder creation failed. *)
end

(** Video codecs. *)
module Video : sig
  (** Video codec ids *)
  type id = Codec_id.video

  (** Return the name of the codec. *)
  val get_name : id -> string

  val find_encoder_id : string -> id
  (** Return the id of a encoder codec from its name.
      @raise Error if the codec is not found or is not an audio codec. *)

  val find_decoder_id : string -> id
  (** Return the id of a encoder codec from its name.
      @raise Error if the codec is not found or is not an audio codec. *)

  (** Return the list of supported frame rates of the codec. *)
  val get_supported_frame_rates : id -> Avutil.rational list
      
  val find_best_frame_rate : id -> Avutil.rational -> Avutil.rational
  (** [Avcodec.Video.find_best_frame_rate id default] return the best frame rate of the [id] codec or the [default] value if the codec has no frame rate. *)

  (** Return the list of supported pixel formats of the codec. *)
  val get_supported_pixel_formats : id -> Avutil.Pixel_format.t list
      
  val find_best_pixel_format : id -> Avutil.Pixel_format.t -> Avutil.Pixel_format.t
  (** [Avcodec.Video.find_best_pixel_format id default] return the best pixel format of the [id] codec or the [default] value if the codec has no pixel format. *)


  (** Return the id of the codec. *)
  val get_id : video t -> id

  (** Returns the width set for the codec. *)
  val get_width : video t -> int

  (** Returns the height set for the codec. *)
  val get_height : video t -> int

  (** Returns the sample aspect ratio set for the codec. *)
  val get_sample_aspect_ratio : video t -> Avutil.rational

  (** Returns the pixel format set for the codec. *)
  val get_pixel_format : video t -> Avutil.Pixel_format.t

  (** Returns the bit rate set for the codec. *)
  val get_bit_rate : video t -> int


  val create_parser : id -> video Packet.parser
  (** [Avcodec.Video.create_parser id] create an video packet parser.
      @raise Error if the parser creation failed. *)

  val create_decoder : id -> video decoder
  (** [Avcodec.Video.create_decoder id] create a video decoder.
      @raise Error if the decoder creation failed. *)

  val create_encoder : ?bit_rate:int -> ?frame_rate:int -> id -> video encoder
  (** [Avcodec.Video.create_encoder ~bit_rate:bit_rate id] create a video encoder.
      @raise Error if the encoder creation failed. *)
end

(** Subtitle codecs. *)
module Subtitle : sig
  (** Subtitle codec ids *)
  type id = Codec_id.subtitle

  (** Return the name of the codec. *)
  val get_name : id -> string

  val find_encoder_id : string -> id
  (** Return the id of a encoder codec from its name.
      @raise Error if the codec is not found or is not an audio codec. *)

  val find_decoder_id : string -> id
  (** Return the id of a encoder codec from its name.
      @raise Error if the codec is not found or is not an audio codec. *)

  (** Return the id of the codec. *)
  val get_id : subtitle t -> id
end


val decode : 'media decoder -> ('media frame -> unit) -> 'media Packet.t -> unit
(** [Avcodec.decode decoder f packet] applies function [f] to the decoded frames frome the [packet] according to the [decoder] configuration.
    @raise Error if the decoding failed. *)

val flush_decoder : 'media decoder -> ('media frame -> unit) -> unit
(** [Avcodec.flush_decoder decoder f] applies function [f] to the decoded frames frome the buffered packets in the [decoder].
    @raise Error if the decoding failed. *)

val encode : 'media encoder -> ('media Packet.t -> unit) -> 'media frame -> unit
(** [Avcodec.encode encoder f frame] applies function [f] to the encoded packets from the [frame] according to the [encoder] configuration.
    @raise Error if the encoding failed. *)

val flush_encoder : 'media encoder -> ('media Packet.t -> unit) -> unit
(** [Avcodec.flush_encoder encoder] applies function [f] to the encoded packets from the buffered frames in the [encoder].
    @raise Error if the encoding failed. *)
