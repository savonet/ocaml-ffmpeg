(** Common code shared across all FFmpeg libraries. *)

(** {1 Line} *)

type input
type output

(** {1 Container} *)

type 'a container

(** {1 Media types} *)

type audio = [ `Audio ]
type video = [ `Video ]
type subtitle = [ `Subtitle ]

type media_type =
  [ audio | video | subtitle | `Unknown | `Data | `Attachment | `Nb ]

(** {1 Format} *)

type ('line, 'media) format

(** {1 Frame} *)

type 'media frame

(** [Avutil.frame_pts frame] returns the presentation timestamp in time_base
    units (time when frame should be shown to user). *)
val frame_pts : _ frame -> Int64.t option

(** [Avutil.frame_set_pts frame pts] sets the presentation time for this frame. *)
val frame_set_pts : _ frame -> Int64.t option -> unit

(** [Avutil.frame_best_effort_timestamp frame] returns the frame timestamp estimated using various heuristics, in stream time base *)
val frame_best_effort_timestamp : _ frame -> Int64.t option

(** duration of the corresponding packet, expressed in AVStream->time_base units. *)
val frame_pkt_duration : _ frame -> Int64.t option

(** [Avutil.frame_copy src dst] copies data from [src] into [dst] *)
val frame_copy : 'a frame -> 'b frame -> unit

(** {1 Exception} *)

(** Internal errors. *)
type error =
  [ `Bsf_not_found
  | `Decoder_not_found
  | `Demuxer_not_found
  | `Encoder_not_found
  | `Eof
  | `Exit
  | `Filter_not_found
  | `Invalid_data
  | `Muxer_not_found
  | `Option_not_found
  | `Patch_welcome
  | `Protocol_not_found
  | `Stream_not_found
  | `Bug
  | `Eagain
  | `Unknown
  | `Experimental
  | `Other of int
  | (* `Failure is for errors from the binding code itself. *)
    `Failure of string ]

exception Error of error

val string_of_error : error -> string

type data =
  (int, Bigarray.int8_unsigned_elt, Bigarray.c_layout) Bigarray.Array1.t

val create_data : int -> data

type rational = { num : int; den : int }

val string_of_rational : rational -> string

(** {5 Constants} *)

val qp2lambda : int

(** {5 Timestamp} *)

(** Formats for time. *)
module Time_format : sig
  (** Time formats. *)
  type t = [ `Second | `Millisecond | `Microsecond | `Nanosecond ]
end

(** Return the time base of FFmpeg. *)
val time_base : unit -> rational

(** {5 Logging utilities} *)

module Log : sig
  type level =
    [ `Quiet | `Panic | `Fatal | `Error | `Warning | `Info | `Verbose | `Debug ]

  val set_level : level -> unit
  val set_callback : (string -> unit) -> unit
  val clear_callback : unit -> unit
end

(** {5 Audio utilities} *)

(** Formats for channels layouts. *)
module Channel_layout : sig
  (** Channel layout formats. *)
  type t = Channel_layout.t

  (** Return a channel layout id that matches name. Raises [Not_found]
      otherwise. name can be one or several of the following notations,
      separated by '+' or '|':

      - the name of an usual channel layout (mono, stereo, 4.0, quad, 5.0,
        5.0(side), 5.1, 5.1(side), 7.1, 7.1(wide), downmix);
      - the name of a single channel (FL, FR, FC, LFE, BL, BR, FLC, FRC, BC, SL,
        SR, TC, TFL, TFC, TFR, TBL, TBC, TBR, DL, DR);
      - a number of channels, in decimal, optionally followed by 'c', yielding
        the default channel layout for that number of channels;
      - a channel layout mask, in hexadecimal starting with "0x" (see the
        AV_CH_* macros). *)
  val find : string -> t

  (** Return a description of the channel layout. *)
  val get_description : ?channels:int -> t -> string

  (** Return the number of channels in the channel layout. *)
  val get_nb_channels : t -> int

  (** Return default channel layout for a given number of channels. Raises
      [Not_found] if not found. *)
  val get_default : int -> t

  (** Return the internal ID for a channel layout. This number should be passed
      as the "channel_layout" [opts] in [Av.new_audio_stream] .*)
  val get_id : t -> int64
end

(** Formats for audio samples. *)
module Sample_format : sig
  (** Audio sample formats. *)
  type t = Sample_format.t

  (** Return the name of the sample format if it exists. *)
  val get_name : t -> string option

  (** Find a sample format by its name. Raises [Not_found] when none exist. *)
  val find : string -> t

  (** Return the internal ID of the sample format. *)
  val get_id : t -> int

  (** Find a sample format from its ID. Raises [Not_found] when none exist. *)
  val find_id : int -> t
end

(** {5 Video utilities} *)

(** Formats for pixels. *)
module Pixel_format : sig
  (** Pixels formats. *)
  type t = Pixel_format.t

  (** Pixel format flags. *)
  type flag = Pixel_format_flag.t

  (** Pixel format component descriptor *)
  type component_descriptor = {
    plane : int;
    step : int;
    offset : int;
    shift : int;
    depth : int;
  }

  (** Pixel format descriptor. *)
  type descriptor = private {
    name : string;
    nb_components : int;
    log2_chroma_w : int;
    log2_chroma_h : int;
    flags : flag list;
    comp : component_descriptor list;
    alias : string option;
  }

  (** Return the pixel's format descriptor. Raises [Not_found]
      if descriptor could not be found. *)
  val descriptor : t -> descriptor

  (** Return the number of bits of the pixel format. *)
  val bits : descriptor -> int

  (** Return the number of planes of the pixel format. *)
  val planes : t -> int

  (** [Pixel_format.to_string f] Return a string representation of the pixel
      format [f] if it exists *)
  val to_string : t -> string option

  (** [Pixel_format.of_string s] Convert the string [s] into a [Pixel_format.t].
      Raises Error if [s] is not a valid format. *)
  val of_string : string -> t

  (** Return the internal ID of the pixel format. *)
  val get_id : t -> int

  (** Find a sample pixel from its ID. Raises [Not_found] when none exist. *)
  val find_id : int -> t
end

module Audio : sig
  (** [Avutil.Audio.create_frame sample_format channel_layout sample_rate samples]
      allocates a new audio frame. *)
  val create_frame :
    Sample_format.t -> Channel_layout.t -> int -> int -> audio frame

  (** [Avutil.Audio.frame_get_sample_format frame] returns the sample format of
      the current frame. *)
  val frame_get_sample_format : audio frame -> Sample_format.t

  (** [Avutil.Audio.frame_get_sample_rate frame] returns the sample rate of the
      current frame. *)
  val frame_get_sample_rate : audio frame -> int

  (** [Avutil.Audio.frame_get_channels frame] returns the number of audio
      channels in the current frame. *)
  val frame_get_channels : audio frame -> int

  (** [Avutil.Audio.frame_get_channel_layout frame] returns the channel layout
      for the current frame. *)
  val frame_get_channel_layout : audio frame -> Channel_layout.t

  (** [Avutil.Audio.frame_nb_samples frame] returns the number of audio samples
      per channel in the current frame. *)
  val frame_nb_samples : audio frame -> int

  (** [Abutil.Audio.frame_copy_samples src src_offset dst dst_offset len] copies
      [len] samples from [src] starting at position [src_offset] into [dst] starting
      at position [dst_offset]. *)
  val frame_copy_samples :
    audio frame -> int -> audio frame -> int -> int -> unit
end

module Video : sig
  type planes = (data * int) array

  (** [Avutil.Video.create_frame w h pf] create a video frame with [w] width,
      [h] height and [pf] pixel format. Raises Error if the allocation failed. *)
  val create_frame : int -> int -> Pixel_format.t -> video frame

  (** [Avutil.Video.frame_get_linesize vf n] return the line size of the [n]
      plane of the [vf] video frame. Raises Error if [n] is out of boundaries. *)
  val frame_get_linesize : video frame -> int -> int

  (** [Avutil.Video.frame_visit ~make_writable:wrt f vf] call the [f] function
      with planes wrapping the [vf] video frame data. The make_writable:[wrt]
      parameter must be set to true if the [f] function writes in the planes.
      Access to the frame through the planes is safe as long as it occurs in the
      [f] function and the frame is not sent to an encoder. The same frame is
      returned for convenience. Raises Error if the make frame writable
      operation failed. *)
  val frame_visit :
    make_writable:bool -> (planes -> unit) -> video frame -> video frame

  (** [Avutil.Video.frame_get_width frame] returns the frame width *)
  val frame_get_width : video frame -> int

  (** [Avutil.Video.frame_get_height frame] returns the frame height *)
  val frame_get_height : video frame -> int

  (** [Avutil.Video.frame_get_pixel_format frame] returns frame's pixel format. *)
  val frame_get_pixel_format : video frame -> Pixel_format.t

  (** [Avutil.Video.frame_get_pixel_aspect frame] returns the frame's pixel aspect. *)
  val frame_get_pixel_aspect : video frame -> rational option
end

(** {5 Subtitle utilities} *)

module Subtitle : sig
  (** Return the time base for subtitles. *)
  val time_base : unit -> rational

  (** [Avutil.Subtitle.create_frame start end lines] create a subtitle frame
      from [lines] which is displayed at [start] time and hidden at [end] time
      in seconds. Raises Error if the allocation failed. *)
  val create_frame : float -> float -> string list -> subtitle frame

  (** Convert subtitle frame to lines. The two float are the start and the end
      dislpay time in seconds. *)
  val frame_to_lines : subtitle frame -> float * float * string list
end

(** {5 Options} *)
module Options : sig
  type t

  type flag =
    [ `Encoding_param
    | `Decoding_param
    | `Audio_param
    | `Video_param
    | `Subtitle_param
    | `Export
    | `Readonly
    | `Bsf_param
    | `Runtime_param
    | `Filtering_param
    | `Deprecated
    | `Child_consts ]

  type 'a entry = {
    default : 'a option;
    (* Used only for numerical options. *)
    min : 'a option;
    max : 'a option;
    (* Pre-defined options. *)
    values : (string * 'a) list;
  }

  type spec =
    [ `Flags of int64 entry
    | `Int of int entry
    | `Int64 of int64 entry
    | `Float of float entry
    | `Double of float entry
    | `String of string entry
    | `Rational of rational entry
    | `Binary of string entry
    | `Dict of string entry
    | `UInt64 of int64 entry
    | `Image_size of string entry
    | `Pixel_fmt of Pixel_format.t entry
    | `Sample_fmt of Sample_format.t entry
    | `Video_rate of string entry
    | `Duration of int64 entry
    | `Color of string entry
    | `Channel_layout of Channel_layout.t entry
    | `Bool of bool entry ]

  type opt = {
    name : string;
    help : string option;
    flags : flag list;
    spec : spec;
  }

  val opts : t -> opt list

  (* Generic type for any object with options. *)
  type obj
  type 'a getter = ?search_children:bool -> name:string -> obj -> 'a

  val get_string : string getter
  val get_int : int getter
  val get_int64 : int64 getter
  val get_float : float getter
  val get_rational : rational getter
  val get_image_size : (int * int) getter
  val get_pixel_fmt : Pixel_format.t getter
  val get_sample_fmt : Sample_format.t getter
  val get_video_rate : rational getter
  val get_channel_layout : Channel_layout.t getter
  val get_dictionary : (string * string) list getter
end

(* {1 Options } *)

type value =
  [ `String of string | `Int of int | `Int64 of int64 | `Float of float ]

type opts = (string, value) Hashtbl.t

val opts_default : opts option -> opts
val mk_opts_array : opts -> (string * string) array
val string_of_opts : opts -> string

val mk_audio_opts :
  ?opts:opts ->
  ?channels:int ->
  ?channel_layout:Channel_layout.t ->
  sample_rate:int ->
  sample_format:Sample_format.t ->
  time_base:rational ->
  unit ->
  opts

val mk_video_opts :
  ?opts:opts ->
  ?frame_rate:rational ->
  pixel_format:Pixel_format.t ->
  width:int ->
  height:int ->
  time_base:rational ->
  unit ->
  opts

val filter_opts : string array -> opts -> unit

(** {5 HwContext} *)

module HwContext : sig
  (** Codec hardward device type. *)
  type device_type = Hw_device_type.t

  (** Device context. *)
  type device_context

  (** Frame context. *)
  type frame_context

  val create_device_context :
    ?device:string -> ?opts:opts -> device_type -> device_context

  val create_frame_context :
    width:int ->
    height:int ->
    src_pixel_format:Pixel_format.t ->
    dst_pixel_format:Pixel_format.t ->
    device_context ->
    frame_context
end
