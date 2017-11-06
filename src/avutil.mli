(** Common code shared across all FFmpeg libraries. *)

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

(** A failure occured (with given explanation). *)
exception Failure of string


(** {9 Timestamp} *)

(** Formats for time. *)
module Time_format : sig

  (** Time formats. *)
  type t =
    | Second
    | Millisecond
    | Microsecond
    | Nanosecond
end

(** Return the time base of FFmpeg. *)
val time_base : unit -> int64



(** {5 Audio utilities} *)

(** Formats for channels layouts. *)
module Channel_layout : sig

  (** Channel layout formats *)
  type t =
    | CL_mono
    | CL_stereo
    | CL_2point1
    | CL_2_1
    | CL_surround
    | CL_3point1
    | CL_4point0
    | CL_4point1
    | CL_2_2
    | CL_quad
    | CL_5point0
    | CL_5point1
    | CL_5point0_back
    | CL_5point1_back
    | CL_6point0
    | CL_6point0_front
    | CL_hexagonal
    | CL_6point1
    | CL_6point1_back
    | CL_6point1_front
    | CL_7point0
    | CL_7point0_front
    | CL_7point1
    | CL_7point1_wide
    | CL_7point1_wide_back
    | CL_octagonal
    (*  | CL_hexadecagonal*)
    | CL_stereo_downmix
end

(** Formats for audio samples. *)
module Sample_format : sig

  (** Audio sample formats *)
  type t =
    | SF_none
    | SF_U8
    | SF_S16
    | SF_S32
    | SF_FLT
    | SF_DBL
    | SF_U8P
    | SF_S16P
    | SF_S32P
    | SF_FLTP
    | SF_DBLP

  (** Return the name of the sample format. *)
  val get_name : t -> string
end


(** {5 Video utilities} *)

(** Formats for pixels. *)
module Pixel_format : sig

  (** Pixels formats. *)
  type t =
    | YUV420p
    | YUYV422
    | RGB24
    | BGR24
    | YUV422p
    | YUV444p
    | YUV410p
    | YUV411p
    | YUVJ422p
    | YUVJ444p
    | RGBA
    | BGRA

  (** Return the number of bits of the pixel format. *)
  val bits : (*?padding:bool ->*) t -> int
end

module Video : sig

  val create_frame : int -> int -> Pixel_format.t -> video frame
  (** [Avutil.Video.create_frame w h pf] create a video frame with [w] width, [h] height and [pf] pixel format. @raise Failure if the allocation failed. *)

  val frame_get : video frame -> int -> int -> int -> int
  (** [Avutil.Video.frame_get p x y] return the [p] plane data of the [x] [y] pixel. @raise Failure if [p], [x] or [y] are out of boundaries. *)

  val frame_set : video frame -> int -> int -> int -> int -> unit
  (** [Avutil.Video.frame_set p x y] set the [p] plane data of the [x] [y] pixel. @raise Failure if [p], [x] or [y] are out of boundaries. *)
end


(** {5 Subtitle utilities} *)

module Subtitle : sig

  val time_base : unit -> int64
  (** Return the time base for subtitles. *)

  val create_frame : float -> float -> string list -> subtitle frame
  (** [Avutil.Subtitle.create_frame start end lines] create a subtitle frame from [lines] which is displayed at [start] time and hidden at [end] time in seconds. @raise Failure if the allocation failed. *)

  val frame_to_lines : subtitle frame -> (float * float * string list)
  (** Convert subtitle frame to lines. The two float are the start and the end dislpay time in seconds. *)
end
