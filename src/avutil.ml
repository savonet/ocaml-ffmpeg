
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
  type t = Pixel_format.t

  external bits : t -> int = "ocaml_avutil_bits_per_pixel"

  let bits (*?(padding=true)*) p =
    let n = bits p in
    (* TODO: when padding is true we should add the padding *)
    n
end

module Time_format = struct
  type t = [
    | `second
    | `millisecond
    | `microsecond
    | `nanosecond
  ]
end

external time_base : unit -> int64 = "ocaml_avutil_time_base"


module Channel_layout = struct
  type t = Channel_layout.t
end

module Sample_format = struct
  type t = Sample_format.t

  external get_name : t -> string = "ocaml_avutil_get_sample_fmt_name"
end


module Video = struct
  type bba = (int, Bigarray.int8_unsigned_elt, Bigarray.c_layout) Bigarray.Array1.t
  type bigarray_planes = (bba * int) array
  type bytes_planes = (bytes * int) array
  type plane = bba * int * video frame

  external create_frame : int -> int -> Pixel_format.t -> video frame = "ocaml_avutil_video_create_frame"

  external frame_get : video frame -> int -> int -> int -> int = "ocaml_avutil_video_frame_get"[@@noalloc]

  external frame_set : video frame -> int -> int -> int -> int -> unit = "ocaml_avutil_video_frame_set"[@@noalloc]


  external frame_get_linesize : video frame -> int -> int = "ocaml_avutil_video_frame_get_linesize"[@@noalloc]

  external frame_planar_get : video frame -> (int [@untagged]) -> (int [@untagged]) -> (int [@untagged]) = "ocaml_avutil_video_frame_planar_get_byte" "ocaml_avutil_video_frame_planar_get"[@@noalloc]

  external frame_planar_set : video frame -> (int [@untagged]) -> (int [@untagged]) -> (int [@untagged]) -> unit = "ocaml_avutil_video_frame_planar_set_byte" "ocaml_avutil_video_frame_planar_set"[@@noalloc]

  
  external copy_frame_to_bigarray_planes : video frame -> bigarray_planes = "ocaml_avutil_video_copy_frame_to_bigarray_planes"

  external copy_bigarray_planes_to_frame : bigarray_planes -> video frame -> unit = "ocaml_avutil_video_copy_bigarray_planes_to_frame"[@@noalloc]

  
  external copy_frame_to_bytes_planes : video frame -> bytes_planes = "ocaml_avutil_video_copy_frame_to_bytes_planes"

  external copy_bytes_planes_to_frame : bytes_planes -> video frame -> unit = "ocaml_avutil_video_copy_bytes_planes_to_frame"[@@noalloc]

  
  external get_frame_planes : video frame -> plane array = "ocaml_avutil_video_get_frame_planes"

  let frame_plane_get_linesize (_, linesize, _) = linesize

  let frame_plane_get (ba, _, _) i = Bigarray.Array1.unsafe_get ba i [@@inline]

  let frame_plane_set (ba, _, _) i v = Bigarray.Array1.unsafe_set ba i v [@@inline]

  
  external get_frame_bigarray_planes : video frame -> bool -> bigarray_planes = "ocaml_avutil_video_get_frame_bigarray_planes"

  let frame_visit ~make_writable visit frame = visit(get_frame_bigarray_planes frame make_writable)
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
