(** This module contains decoders and encoders for audio, video and subtitle
    codecs. *)

open Avutil

type ('media, 'mode) codec
type 'media params
type 'media decoder
type 'media encoder
type encode = [ `Encoder ]
type decode = [ `Decoder ]

(** Get the params of a given encoder *)
val params : 'media encoder -> 'media params

(** Get the time base of a given encoder. *)
val time_base : 'media encoder -> Avutil.rational

(** Get the name of a given codec. *)
val name : _ codec -> string

(** Codec capabilities. *)
type capability = Codec_capabilities.t

(** Get the encoding capabilities for this codec. *)
val capabilities : ([< `Audio | `Video ], encode) codec -> capability list

(** Codec hardware config method. *)
type hw_config_method = Hw_config_method.t

(** Hardward config for the given codec. *)
type hw_config = {
  pixel_format : Pixel_format.t;
  methods : hw_config_method list;
  device_type : HwContext.device_type;
}

(** Get the codec's hardward configs. *)
val hw_configs : ([< `Audio | `Video ], _) codec -> hw_config list

(** Packet. *)
module Packet : sig
  (** Packet type *)
  type 'media t

  (** Parser type *)
  type 'media parser

  (** Packet flags *)
  type flag = [ `Keyframe | `Corrupt | `Discard | `Trusted | `Disposable ]

  (** Return a fresh packet refereing the same data. *)
  val dup : 'media t -> 'media t

  (** Retun the packet flags. *)
  val get_flags : 'media t -> flag list

  (** Return the size of the packet. *)
  val get_size : 'media t -> int

  (** Return the stream index of the packet. *)
  val get_stream_index : 'media t -> int

  (** Set the stream index of the packet. *)
  val set_stream_index : 'media t -> int -> unit

  (** Return the packet PTS (Presentation Time) in its
      stream's base_time unit. *)
  val get_pts : 'media t -> Int64.t option

  (** Set the packet PTS (Presentation Time) in its
      stream's base_time unit. *)
  val set_pts : 'media t -> Int64.t option -> unit

  (** Return the packet DTS (Decoding Time) in its
      stream's base_time unit. *)
  val get_dts : 'media t -> Int64.t option

  (** Set the packet DTS (Decoding Time) in its
      stream's base_time unit. *)
  val set_dts : 'media t -> Int64.t option -> unit

  (** Return the packet duration in its
      stream's base_time unit.*)
  val get_duration : 'media t -> Int64.t option

  (** Set the packet duration in its
      stream's base_time unit.*)
  val set_duration : 'media t -> Int64.t option -> unit

  (** Return the packet byte position in stream. *)
  val get_position : 'media t -> Int64.t option

  (** Set the packet byte position in stream. *)
  val set_position : 'media t -> Int64.t option -> unit

  (** Return a fresh bytes array containing a copy of packet datas. *)
  val to_bytes : 'media t -> bytes

  (** [Avcodec.Packet.parse_data parser f data] applies function [f] to the
      parsed packets frome the [data] array according to the [parser]
      configuration.

      Raise Error if the parsing failed. *)
  val parse_data : 'media parser -> ('media t -> unit) -> data -> unit

  (** Same as {!Avcodec.Packet.parse_data} with bytes array. *)
  val parse_bytes : 'media parser -> ('media t -> unit) -> bytes -> int -> unit
end

(** Audio codecs. *)
module Audio : sig
  (** Main types for audio codecs. *)
  type 'mode t = (audio, 'mode) codec

  (** Audio codec ids. Careful: different codecs share the same ID, e.g. aac and
      libfdk_aac *)
  type id = Codec_id.audio

  (** Find an encoder from its name.

      Raise Error if the codec is not found or is not an audio codec. *)
  val find_encoder_by_name : string -> encode t

  (** Find an encoder from its id.

      Raise Error if the codec is not found or is not an audio codec. *)
  val find_encoder : id -> encode t

  (** Find a decoder from its name.

      Raise Error if the codec is not found or is not an audio codec. *)
  val find_decoder_by_name : string -> decode t

  (** Find a decoder from its id.

      Raise Error if the codec is not found or is not an audio codec. *)
  val find_decoder : id -> decode t

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

  (** [Avcodec.Audio.create_parser codec] create an audio packet parser.

      Raise Error if the parser creation failed. *)
  val create_parser : decode t -> audio Packet.parser

  (** [Avcodec.Audio.create_decoder codec] create an audio decoder.

      Raise Error if the decoder creation failed. *)
  val create_decoder : decode t -> audio decoder

  (** [Avcodec.Audio.create_encoder] create an audio encoder.

      Params have the same semantics as in [Av.new_audio_stream]

      Raise Error if the encoder creation failed. *)
  val create_encoder :
    ?opts:opts ->
    ?channels:int ->
    ?channel_layout:Channel_layout.t ->
    sample_rate:int ->
    sample_format:Avutil.Sample_format.t ->
    time_base:Avutil.rational ->
    encode t ->
    audio encoder

  (** Get the desired frame_size for this encoder. *)
  val frame_size : audio encoder -> int

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
  type 'mode t = (video, 'mode) codec

  (** Video codec ids. Careful: different codecs share the same ID, e.g. aac and
      libfdk_aac *)
  type id = Codec_id.video

  (** Find an encoder from its name.

      Raise Error if the codec is not found or is not an audio codec. *)
  val find_encoder_by_name : string -> encode t

  (** Find an encoder from its id.

      Raise Error if the codec is not found or is not an audio codec. *)
  val find_encoder : id -> encode t

  (** Find a decoder from its name.

      Raise Error if the codec is not found or is not an audio codec. *)
  val find_decoder_by_name : string -> decode t

  (** Find a decoder from its id.

      Raise Error if the codec is not found or is not an audio codec. *)
  val find_decoder : id -> decode t

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

  (** [Avcodec.Video.create_parser codec] create an video packet parser.

      Raise Error if the parser creation failed. *)
  val create_parser : decode t -> video Packet.parser

  (** [Avcodec.Video.create_decoder codec] create a video decoder.

      Raise Error if the decoder creation failed. *)
  val create_decoder : decode t -> video decoder

  type hardware_context =
    [ `Device_context of HwContext.device_context
    | `Frame_context of HwContext.frame_context ]

  (** [Avcodec.Video.create_encoder] create a video encoder.

      Params have the same semantics as in [Av.new_video_stream]

      Raise Error if the encoder creation failed. *)
  val create_encoder :
    ?opts:opts ->
    ?frame_rate:Avutil.rational ->
    ?hardware_context:hardware_context ->
    pixel_format:Avutil.Pixel_format.t ->
    width:int ->
    height:int ->
    time_base:Avutil.rational ->
    encode t ->
    video encoder

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
  val get_pixel_format : video params -> Avutil.Pixel_format.t option

  (** Returns the bit rate set for the codec. *)
  val get_bit_rate : video params -> int
end

(** Subtitle codecs. *)
module Subtitle : sig
  (** Main subtitle types. *)
  type 'mode t = (subtitle, 'mode) codec

  (** Subtitle codec ids. Careful: different codecs share the same ID, e.g. aac
      and libfdk_aac *)
  type id = Codec_id.subtitle

  (** Find an encoder from its name.

      Raise Error if the codec is not found or is not an audio codec. *)
  val find_encoder_by_name : string -> encode t

  (** Find an encoder from its id.

      Raise Error if the codec is not found or is not an audio codec. *)
  val find_encoder : id -> encode t

  (** Find a decoder from its name.

      Raise Error if the codec is not found or is not an audio codec. *)
  val find_decoder_by_name : string -> decode t

  (** Find a decoder from its id.

      Raise Error if the codec is not found or is not an audio codec. *)
  val find_decoder : id -> decode t

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
