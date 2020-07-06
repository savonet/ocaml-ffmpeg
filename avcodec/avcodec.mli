(** This module contains decoders and encoders for audio, video and subtitle
    codecs. *)

open Avutil

type 'a params
type 'media decoder
type 'media encoder

(** Codec capabilities. *)
type capability = Codec_capabilities.t

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

  (** Return the packet PTS (Presentation Time) *)
  val get_pts : 'a t -> Int64.t option

  (** Set the packet PTS (Presentation Time) *)
  val set_pts : 'a t -> Int64.t option -> unit

  (** Return the packet DTS (Decoding Time) *)
  val get_dts : 'a t -> Int64.t option

  (** Set the packet DTS (Decoding Time) *)
  val set_dts : 'a t -> Int64.t option -> unit

  (** Size in bytes. *)
  val size : 'a t -> int

  (** Return a fresh bytes array containing a copy of packet datas. *)
  val to_bytes : 'a t -> bytes

  (** [Avcodec.Packet.parse_data parser f data] applies function [f] to the
      parsed packets frome the [data] array according to the [parser]
      configuration.

      Raise Error if the parsing failed. *)
  val parse_data : 'a parser -> ('a t -> unit) -> data -> unit

  (** Same as {!Avcodec.Packet.parse_data} with bytes array. *)
  val parse_bytes : 'a parser -> ('a t -> unit) -> bytes -> int -> unit
end

(** Audio codecs. *)
module Audio : sig
  (** Main types for audio codecs. *)
  type 'a t

  (** Find an encoder from its name.

      Raise Error if the codec is not found or is not an audio codec. *)
  val find_encoder : string -> [ `Encoder ] t

  (** Find a decoder from its name.

      Raise Error if the codec is not found or is not an audio codec. *)
  val find_decoder : string -> [ `Decoder ] t

  (** Return the list of supported channel layouts of the codec. *)
  val get_supported_channel_layouts : _ t -> Avutil.Channel_layout.t list

  (** [Avcodec.Audio.find_best_channel_layout codec default] return the best
      channel layout of the [codec] codec or the [default] value if the codec
      has no channel layout. *)
  val find_best_channel_layout :
    _ t -> Avutil.Channel_layout.t -> Avutil.Channel_layout.t

  (** Return the list of supported sample formats of the codec. *)
  val get_supported_sample_formats : _ t -> Avutil.Sample_format.t list

  (** [Avcodec.Audio.find_best_sample_format codec default] return the best
      sample format of the [codec] codec or the [default] value if the codec has
      no sample format. *)
  val find_best_sample_format :
    _ t -> Avutil.Sample_format.t -> Avutil.Sample_format.t

  (** Return the list of supported sample rates of the codec. *)
  val get_supported_sample_rates : _ t -> int list

  (** [Avcodec.Audio.find_best_sample_rate codec default] return the best sample
      rate of the [codec] codec or the [default] value if the codec has no
      sample rate. *)
  val find_best_sample_rate : _ t -> int -> int

  (** Get the encoding capabilities for this codec. *)
  val capabilities : [ `Encoder ] t -> capability list

  (** [Avcodec.Audio.create_parser codec] create an audio packet parser.

      Raise Error if the parser creation failed. *)
  val create_parser : [ `Decoder ] t -> audio Packet.parser

  (** [Avcodec.Audio.create_decoder codec] create an audio decoder.

      Raise Error if the decoder creation failed. *)
  val create_decoder : [ `Decoder ] t -> audio decoder

  (** [Avcodec.Audio.create_encoder ~bitrate:bitrate codec] create an audio
      encoder.

      Raise Error if the encoder creation failed. *)
  val create_encoder :
    ?bit_rate:int ->
    channel_layout:Avutil.Channel_layout.t ->
    channels:int ->
    sample_format:Avutil.Sample_format.t ->
    sample_rate:int ->
    [ `Encoder ] t ->
    audio encoder

  (** Get the desired frame_size for this encoder. *)
  val frame_size : audio encoder -> int

  (** Audio codec ids. Careful: different codecs share the same ID, e.g. aac and
      libfdk_aac *)
  type id = Codec_id.audio

  (** Return the name of the codec ID. *)
  val string_of_id : id -> string

  (** Return the ID (class) of a codec. *)
  val get_id : _ t -> id

  (** Return the id of the codec params. *)
  val get_params_id : audio params -> id

  (** Return the channel layout set for the codec params. *)
  val get_channel_layout : audio params -> Avutil.Channel_layout.t

  (** Returns the number of channels set for the codec params. *)
  val get_nb_channels : audio params -> int

  (** Returns the sample format set for the codec params. *)
  val get_sample_format : audio params -> Avutil.Sample_format.t

  (** Returns the bit rate set for the codec params. *)
  val get_bit_rate : audio params -> int

  (** Returns the sample rate set for the codec. *)
  val get_sample_rate : audio params -> int
end

(** Video codecs. *)
module Video : sig
  (** Main types for video codecs. *)
  type 'a t

  (** Find an encoder from its name.

      Raise Error if the codec is not found or is not an audio codec. *)
  val find_encoder : string -> [ `Encoder ] t

  (** Find a decoder from its name.

      Raise Error if the codec is not found or is not an audio codec. *)
  val find_decoder : string -> [ `Decoder ] t

  (** Return the list of supported frame rates of the codec. *)
  val get_supported_frame_rates : _ t -> Avutil.rational list

  (** [Avcodec.Video.find_best_frame_rate codec default] return the best frame
      rate of the [codec] codec or the [default] value if the codec has no frame
      rate. *)
  val find_best_frame_rate : _ t -> Avutil.rational -> Avutil.rational

  (** Return the list of supported pixel formats of the codec. *)
  val get_supported_pixel_formats : _ t -> Avutil.Pixel_format.t list

  (** [Avcodec.Video.find_best_pixel_format codec default] return the best pixel
      format of the [codec] codec or the [default] value if the codec has no
      pixel format. *)
  val find_best_pixel_format :
    _ t -> Avutil.Pixel_format.t -> Avutil.Pixel_format.t

  (** Get the encoding capabilities for this codec. *)
  val capabilities : [ `Encoder ] t -> capability list

  (** [Avcodec.Video.create_parser codec] create an video packet parser.

      Raise Error if the parser creation failed. *)
  val create_parser : [ `Decoder ] t -> video Packet.parser

  (** [Avcodec.Video.create_decoder codec] create a video decoder.

      Raise Error if the decoder creation failed. *)
  val create_decoder : [ `Decoder ] t -> video decoder

  (** [Avcodec.Video.create_encoder ~bit_rate:bit_rate codec] create a video
      encoder.

      Raise Error if the encoder creation failed. *)
  val create_encoder :
    ?bit_rate:int ->
    width:int ->
    height:int ->
    pixel_format:Avutil.Pixel_format.t ->
    framerate:Avutil.rational ->
    [ `Encoder ] t ->
    video encoder

  (** Video codec ids. Careful: different codecs share the same ID, e.g. aac and
      libfdk_aac *)
  type id = Codec_id.video

  (** Return the name of the codec. *)
  val string_of_id : id -> string

  (** Return the ID (class) of a codec. *)
  val get_id : _ t -> id

  (** Return the id of the codec params. *)
  val get_params_id : video params -> id

  (** Returns the width set for the codec params. *)
  val get_width : video params -> int

  (** Returns the height set for the codec params. *)
  val get_height : video params -> int

  (** Returns the sample aspect ratio set for the codec params. *)
  val get_sample_aspect_ratio : video params -> Avutil.rational

  (** Returns the pixel format set for the codec params. *)
  val get_pixel_format : video params -> Avutil.Pixel_format.t

  (** Returns the bit rate set for the codec. *)
  val get_bit_rate : video params -> int
end

(** Subtitle codecs. *)
module Subtitle : sig
  (** Main subtitle types. *)
  type 'a t

  (** Find an encoder from its name.

      Raise Error if the codec is not found or is not an audio codec. *)
  val find_encoder : string -> [ `Encoder ] t

  (** Find a decoder from its name.

      Raise Error if the codec is not found or is not an audio codec. *)
  val find_decoder : string -> [ `Decoder ] t

  (** Subtitle codec ids. Careful: different codecs share the same ID, e.g. aac
      and libfdk_aac *)
  type id = Codec_id.subtitle

  (** Return the name of the codec. *)
  val string_of_id : id -> string

  (** Return the ID (class) of a codec. *)
  val get_id : _ t -> id

  (** Return the id of the codec params. *)
  val get_params_id : subtitle params -> id
end

(** [Avcodec.decode decoder f packet] applies function [f] to the decoded frames
    frome the [packet] according to the [decoder] configuration.

    Raise Error if the decoding failed. *)
val decode : 'media decoder -> ('media frame -> unit) -> 'media Packet.t -> unit

(** [Avcodec.flush_decoder decoder f] applies function [f] to the decoded frames
    frome the buffered packets in the [decoder].

    Raise Error if the decoding failed. *)
val flush_decoder : 'media decoder -> ('media frame -> unit) -> unit

(** [Avcodec.encode encoder f frame] applies function [f] to the encoded packets
    from the [frame] according to the [encoder] configuration.

    Raise Error if the encoding failed. *)
val encode : 'media encoder -> ('media Packet.t -> unit) -> 'media frame -> unit

(** [Avcodec.flush_encoder encoder] applies function [f] to the encoded packets
    from the buffered frames in the [encoder].

    Raise Error if the encoding failed. *)
val flush_encoder : 'media encoder -> ('media Packet.t -> unit) -> unit
