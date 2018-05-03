
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


type data = (int, Bigarray.int8_unsigned_elt, Bigarray.c_layout) Bigarray.Array1.t

let create_data len = Bigarray.Array1.create Bigarray.int8_unsigned Bigarray.c_layout len


type rational = {num : int; den : int}

external time_base : unit -> rational = "ocaml_avutil_time_base"


module Time_format = struct
  type t = [
    | `Second
    | `Millisecond
    | `Microsecond
    | `Nanosecond
  ]
end


module Pixel_format = struct
  type t = Pixel_format.t

  external bits : t -> int = "ocaml_avutil_pixelformat_bits_per_pixel"
  external planes : t -> int = "ocaml_avutil_pixelformat_planes"
  external to_string : t -> string = "ocaml_avutil_pixelformat_to_string"
  external of_string : string -> t = "ocaml_avutil_pixelformat_of_string"

  let bits (*?(padding=true)*) p =
    let n = bits p in
    (* TODO: when padding is true we should add the padding *)
    n
end

module Channel_layout = struct
  type t = Channel_layout.t
end

module Sample_format = struct
  type t = Sample_format.t

  external get_name : t -> string = "ocaml_avutil_get_sample_fmt_name"
end


module Video = struct
  type planes = (data * int) array

  external create_frame : int -> int -> Pixel_format.t -> video frame = "ocaml_avutil_video_create_frame"

  external frame_get_linesize : video frame -> int -> int = "ocaml_avutil_video_frame_get_linesize"

  external get_frame_planes : video frame -> bool -> planes = "ocaml_avutil_video_get_frame_bigarray_planes"

  let frame_visit ~make_writable visit frame = visit(get_frame_planes frame make_writable); frame
end

module Subtitle = struct
  let time_base() = {num = 1; den = 100}

  external create_frame : int64 -> int64 -> string array -> subtitle frame = "ocaml_avutil_subtitle_create_frame"

  let create_frame start_time end_time lines =
    let num_time_base = float_of_int((time_base()).num) in
    let den_time_base = float_of_int((time_base()).den) in

    create_frame (Int64.of_float(start_time *. den_time_base /. num_time_base))
      (Int64.of_float(end_time *. den_time_base /. num_time_base))
      (Array.of_list lines)


  external frame_to_lines : subtitle frame -> (int64 * int64 * string array) = "ocaml_avutil_subtitle_to_lines"

  let frame_to_lines t =
    let num_time_base = float_of_int((time_base()).num) in
    let den_time_base = float_of_int((time_base()).den) in

    let s, e, lines = frame_to_lines t in

    Int64.(to_float s *. num_time_base /. den_time_base,
           to_float e *. num_time_base /. den_time_base,
           Array.to_list lines)

end
