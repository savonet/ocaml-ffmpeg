(** Common code shared across all FFmpeg libraries. *)

(* {1 Options } *)

type opt_val = [
  | `String of string
  | `Int of int
  | `Float of float
]

type opts = (string, opt_val) Hashtbl.t

(** {1 Line} *)

type input
type output

(** {1 Container} *)

type 'a container


(** {1 Media} *)

type audio
type video
type subtitle


(** {1 Format} *)

type ('line, 'media) format


(** {1 Frame} *)

type 'media frame


(** {1 Exception} *)

(** Internal errors. *)
type error = [
  | `Bsf_not_found
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
(* `Failure is for errors from the binding code itself. *)
  | `Failure of string
]

exception Error of error

val string_of_error : error -> string

type data = (int, Bigarray.int8_unsigned_elt, Bigarray.c_layout) Bigarray.Array1.t

val create_data : int -> data


type rational = {num : int; den : int}

(** {9 Timestamp} *)

(** Formats for time. *)
module Time_format : sig

  (** Time formats. *)
  type t = [
    | `Second
    | `Millisecond
    | `Microsecond
    | `Nanosecond
  ]
end

(** Return the time base of FFmpeg. *)
val time_base : unit -> rational

(** {5 Logging utilities} *)

module Log : sig
  type level = [
    | `Quiet
    | `Panic
    | `Fatal
    | `Error
    | `Warning
    | `Info
    | `Verbose
    | `Debug
  ]

  val set_level      : level -> unit
  val set_callback   : (string -> unit) -> unit
  val clear_callback : unit -> unit
end

(** {5 Audio utilities} *)

(** Formats for channels layouts. *)
module Channel_layout : sig

  (** Channel layout formats. *)
  type t = Channel_layout.t

  (** Return a channel layout id that matches name. @raises [Not_found] otherwise. 
      name can be one or several of the following notations, separated by '+' or '|':
      - the name of an usual channel layout (mono, stereo, 4.0, quad, 5.0, 5.0(side), 5.1, 5.1(side), 7.1, 7.1(wide), downmix);
      - the name of a single channel (FL, FR, FC, LFE, BL, BR, FLC, FRC, BC, SL, SR, TC, TFL, TFC, TFR, TBL, TBC, TBR, DL, DR);
      - a number of channels, in decimal, optionally followed by 'c', yielding the default channel layout for that number of channels;
      - a channel layout mask, in hexadecimal starting with "0x" (see the AV_CH_* macros). *)
  val find : string -> t

  (* Return a description of the channel layout. *)
  val get_description : ?channels:int -> t -> string

  (** Return the number of channels in the channel layout. *)
  val get_nb_channels : t -> int

  (** Return default channel layout for a given number of channels.
      @raises [Not_found] if not found. *)
  val get_default : int -> t

  (** Return the internal ID for a channel layout. This number should be passed as the
      "channel_layout" [opts] in [Av.new_audio_stream] .*)
  val get_id : t -> int
end

(** Formats for audio samples. *)
module Sample_format : sig

  (** Audio sample formats. *)
  type t = Sample_format.t

  (** Return the name of the sample format. *)
  val get_name : t -> string

  (** Find a sample format by its name. @aises [Not_found] when none exist. *)
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

  (** Return the number of bits of the pixel format. *)
  val bits : (*?padding:bool ->*) t -> int

  (** Return the number of planes of the pixel format. *)
  val planes : t -> int

  (** [Pixel_format.to_string f] Return a string representation of the pixel format [f]. *)
  val to_string : t -> string

  (** [Pixel_format.of_string s] Convert the string [s] into a [Pixel_format.t]. @raise Error if [s] is not a valid format. *)
  val of_string : string -> t
end

module Audio : sig
  val frame_get_sample_format : audio frame -> Sample_format.t
  (** [Avutil.Audio.frame_get_sample_format frame] returns the sample format of the current frame. *)
end

module Video : sig
  type planes = (data * int) array

  val create_frame : int -> int -> Pixel_format.t -> video frame
  (** [Avutil.Video.create_frame w h pf] create a video frame with [w] width, [h] height and [pf] pixel format. @raise Error if the allocation failed. *)

  val frame_get_linesize : video frame -> int -> int
  (** [Avutil.Video.frame_get_linesize vf n] return the line size of the [n] plane of the [vf] video frame. @raise Error if [n] is out of boundaries. *)

  val frame_visit : make_writable:bool -> (planes -> unit) -> video frame -> video frame
  (** [Avutil.Video.frame_visit ~make_writable:wrt f vf] call the [f] function with planes wrapping the [vf] video frame data. The make_writable:[wrt] parameter must be set to true if the [f] function writes in the planes. Access to the frame through the planes is safe as long as it occurs in the [f] function and the frame is not sent to an encoder. The same frame is returned for convenience. @raise Error if the make frame writable operation failed. *)

  val frame_pts : video frame -> Int64.t
  (** [Avutil.Video.frame_pts frame] returns the presentation timestamp in time_base units (time when frame should be shown to user). *)
end


(** {5 Subtitle utilities} *)

module Subtitle : sig

  val time_base : unit -> rational
  (** Return the time base for subtitles. *)

  val create_frame : float -> float -> string list -> subtitle frame
  (** [Avutil.Subtitle.create_frame start end lines] create a subtitle frame from [lines] which is displayed at [start] time and hidden at [end] time in seconds. @raise Error if the allocation failed. *)

  val frame_to_lines : subtitle frame -> (float * float * string list)
  (** Convert subtitle frame to lines. The two float are the start and the end dislpay time in seconds. *)
end
