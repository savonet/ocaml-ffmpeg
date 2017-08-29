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

  val get_name : t -> string
end

(** Audio format properties. *)
type audio_format = {
  channel_layout : Channel_layout.t;
  sample_format : Sample_format.t;
  sample_rate : int
}
                    
(** Audio frame type. *)
module Audio_frame : sig
  type t
end

(** Video frame type. *)
module Video_frame : sig
  type t

  val create : int -> int -> Pixel_format.t -> t

  val get : t -> int -> int -> int -> int

  val set : t -> int -> int -> int -> int -> unit
end

(** Subtitle frame type. *)
module Subtitle_frame : sig
  type t
end
