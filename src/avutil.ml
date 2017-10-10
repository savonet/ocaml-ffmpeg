(* Line *)
type input
type output

(* Container *)
type 'a container

(* Media *)
type audio
type video
type subtitle

(* Format *)
type ('line, 'media) format

(* Frame *)
type 'media frame

exception Failure of string

let () =
  Callback.register_exception "ffmpeg_exn_failure" (Failure "");


module Pixel_format = struct
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

  external bits : t -> int = "ocaml_avutil_bits_per_pixel"

  let bits (*?(padding=true)*) p =
    let n = bits p in
    (* TODO: when padding is true we should add the padding *)
    n
end

module Time_format = struct
  type t =
    | Second
    | Millisecond
    | Microsecond
    | Nanosecond
end

external time_base : unit -> int64 = "ocaml_avutil_time_base"


module Channel_layout = struct
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

module Sample_format = struct
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

  external get_name : t -> string = "ocaml_avutil_get_sample_fmt_name"
end


module Media_type = struct
  type t =
  | MT_audio
  | MT_video
  | MT_subtitle
  | MT_data
  | MT_attachment
  | MT_unknown
end


module Video = struct
  external create_frame : int -> int -> Pixel_format.t -> video frame = "ocaml_avutil_video_create_frame"

  external frame_get : video frame -> int -> int -> int -> int = "ocaml_avutil_video_frame_get"(* [@@noalloc]*)

  external frame_set : video frame -> int -> int -> int -> int -> unit = "ocaml_avutil_video_frame_set"(* [@@noalloc]*)
end

module Subtitle = struct
  external time_base : unit -> int64 = "ocaml_avutil_subtitle_time_base"

  external create_frame : int64 -> int64 -> string array -> subtitle frame = "ocaml_avutil_subtitle_create_frame"

  let create_frame start_time end_time lines =
    let time_base = Int64.to_float(time_base()) in
    create_frame (Int64.of_float(start_time *. time_base))
      (Int64.of_float(end_time *. time_base))
      (Array.of_list lines)


  external frame_to_lines : subtitle frame -> (int64 * int64 * string array) = "ocaml_avutil_subtitle_to_lines"

  let frame_to_lines t =
    let s, e, lines = frame_to_lines t in let time_base = Int64.to_float(time_base()) in
    Int64.(to_float s /. time_base, to_float e /. time_base, Array.to_list lines)

end
