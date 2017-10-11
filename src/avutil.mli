(** Common code shared across all FFmpeg libraries. *)

(** Line *)

type input
type output


(** Container *)

type 'a container


(** Media *)

type audio
type video
type subtitle


(** Format *)

type ('line, 'media) format


(** Frame *)

type 'media frame


(** A failure occured (with given explanation). *)
exception Failure of string


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


(** Media type. *)
module Media_type : sig

  (** Media type *)
  type t =
    | MT_audio
    | MT_video
    | MT_subtitle
    | MT_data
    | MT_attachment
    | MT_unknown
end


(** Video utilities. *)
module Video : sig

  (** Create a video frame. *)
  val create_frame : int -> int -> Pixel_format.t -> video frame
  (** Parameters : width height pixel_format *)

  (** Return a data of a pixel in a plane *)
  val frame_get : video frame -> int -> int -> int -> int
  (** Parameters : plane_number x y *)

  (** Set a data of a pixel in a plane *)
  val frame_set : video frame -> int -> int -> int -> int -> unit
  (** Parameters : plane_number x y *)
end

(** Subtitle utilities. *)
module Subtitle : sig

  (** Return the time base for subtitles. *)
  val time_base : unit -> int64

  (** Create a subtitle frame from lines. *)
  val create_frame : float -> float -> string list -> subtitle frame
  (** Parameters : start end lines
   The start and the end are dislpay time in seconds. *)

  (** Convert subtitle frame to lines. *)
  val frame_to_lines : subtitle frame -> (float * float * string list)
  (** The two float are the start and the end dislpay time in seconds. *)
end
