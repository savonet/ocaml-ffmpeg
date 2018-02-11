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
  type t = [
    | `second
    | `millisecond
    | `microsecond
    | `nanosecond
  ]
end

(** Return the time base of FFmpeg. *)
val time_base : unit -> int64



(** {5 Audio utilities} *)

(** Formats for channels layouts. *)
module Channel_layout : sig

  (** Channel layout formats. *)
  type t = Channel_layout.t
end

(** Formats for audio samples. *)
module Sample_format : sig

  (** Audio sample formats. *)
  type t = Sample_format.t

  (** Return the name of the sample format. *)
  val get_name : t -> string
end


(** {5 Video utilities} *)

(** Formats for pixels. *)
module Pixel_format : sig

  (** Pixels formats. *)
  type t = Pixel_format.t

  (** Return the number of bits of the pixel format. *)
  val bits : (*?padding:bool ->*) t -> int
end

module Video : sig
  type bba = (int, Bigarray.int8_unsigned_elt, Bigarray.c_layout) Bigarray.Array1.t
  type bigarray_planes = (bba * int) array
  type bytes_planes = (bytes * int) array
  (* type plane *)
  type plane = bba * int * video frame

  val create_frame : int -> int -> Pixel_format.t -> video frame
  (** [Avutil.Video.create_frame w h pf] create a video frame with [w] width, [h] height and [pf] pixel format. @raise Failure if the allocation failed. *)

  val frame_get : video frame -> int -> int -> int -> int
  (** [Avutil.Video.frame_get p x y] return the [p] plane data of the [x] [y] pixel. @raise Failure if [p], [x] or [y] are out of boundaries. *)

  val frame_set : video frame -> int -> int -> int -> int -> unit
  (** [Avutil.Video.frame_set p x y] set the [p] plane data of the [x] [y] pixel. @raise Failure if [p], [x] or [y] are out of boundaries. *)

  val frame_get_linesize : video frame -> int -> int

  val frame_planar_set : video frame -> int -> int -> int -> unit

  val frame_to_bigarray_planes : video frame -> bigarray_planes

  val bigarray_planes_to_frame : bigarray_planes -> video frame -> unit

  val frame_to_bytes_planes : video frame -> bytes_planes

  val bytes_planes_to_frame : bytes_planes -> video frame -> unit

  val get_frame_planes : video frame -> plane array

  val frame_plane_get_linesize : plane -> int

  val frame_plane_get : plane -> int -> int

  val frame_plane_set : plane -> int -> int -> unit
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
