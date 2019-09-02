open Avutil

external init : unit -> unit = "ocaml_av_init" [@@noalloc]

let () = init ()

(* Format *)
module Format = struct
  external get_input_name : (input, _)format -> string = "ocaml_av_input_format_get_name"
  external get_input_long_name : (input, _)format -> string = "ocaml_av_input_format_get_long_name"

  external get_output_name : (output, _)format -> string = "ocaml_av_output_format_get_name"
  external get_output_long_name : (output, _)format -> string = "ocaml_av_output_format_get_long_name"
  external get_audio_codec_id : (output, audio)format -> Avcodec.Audio.id = "ocaml_av_output_format_get_audio_codec_id"
  external get_video_codec_id : (output, video)format -> Avcodec.Video.id = "ocaml_av_output_format_get_video_codec_id"
  external get_subtitle_codec_id : (output, subtitle)format -> Avcodec.Subtitle.id = "ocaml_av_output_format_get_subtitle_codec_id"

  external guess_output_format : string -> string -> string -> (output, 'a) format option = "ocaml_av_output_format_guess"
  let guess_output_format ?(short_name="") ?(filename="") ?(mime="") () =
    guess_output_format short_name filename mime
end


external finalize_av : _ container -> unit = "ocaml_av_finalize_av"

let () =
  Callback.register "ocaml_av_finalize_av" finalize_av

(* Input *)
external open_input : string -> input container = "ocaml_av_open_input"
external open_input_format : (input, _)format -> input container = "ocaml_av_open_input_format"

type avio

type read = bytes -> int -> int -> int
type write = bytes -> int -> int -> int
type _seek = int -> int -> int
type seek = int -> Unix.seek_command -> int

let seek_of_int = function
  | 0 -> Unix.SEEK_SET
  | 1 -> Unix.SEEK_CUR
  | 2 -> Unix.SEEK_END
  | _ -> assert false;

type read_callbacks = {
  read : read;
  seek : seek option
}

external ocaml_av_create_io : int -> read option -> write option -> _seek option -> avio = "ocaml_av_create_io"

let _seek_of_seek = function
  | None    -> None
  | Some fn -> Some (fun a m -> fn a (seek_of_int m))

let ocaml_av_create_read_io len {read;seek} =
 ocaml_av_create_io len (Some read) None (_seek_of_seek seek)

external ocaml_av_open_input_stream : avio -> input container = "ocaml_av_open_input_stream"
external caml_av_input_io_finalise : avio -> unit = "caml_av_input_io_finalise"

let open_input_stream read_callbacks =
  let avio = ocaml_av_create_read_io 4096 read_callbacks in
  let cleanup () = caml_av_input_io_finalise avio in
  let input =
    ocaml_av_open_input_stream avio
  in
  Gc.finalise_last cleanup input;
  input

external _get_duration : input container -> int -> Time_format.t -> Int64.t = "ocaml_av_get_duration"
let get_input_duration ?(format=`Second) i = _get_duration i (-1) format

external _get_metadata : input container -> int -> (string * string) list = "ocaml_av_get_metadata"
let get_input_metadata i = List.rev(_get_metadata i (-1))


(* Input Stream *)
type ('a, 'b) stream = {container : 'a container; index : int}

type media_type = MT_audio | MT_video | MT_subtitle


let mk_stream container index = {container; index}

external get_codec : (_, 'm)stream -> 'm Avcodec.t = "ocaml_av_get_stream_codec_parameters"
external get_time_base : (_, _)stream -> Avutil.rational = "ocaml_av_get_stream_time_base"
external set_time_base : (_, _)stream -> Avutil.rational -> unit = "ocaml_av_set_stream_time_base"


external _get_streams : input container -> media_type -> int list = "ocaml_av_get_streams"

let get_streams input media_type =
  _get_streams input media_type
  |> List.rev_map(fun i -> let s = mk_stream input i in (i, s, get_codec s))


let get_audio_streams input = get_streams input MT_audio
let get_video_streams input = get_streams input MT_video
let get_subtitle_streams input = get_streams input MT_subtitle


external _find_best_stream : input container -> media_type -> int = "ocaml_av_find_best_stream"

let find_best_stream c t =
  let i = _find_best_stream c t in let s = mk_stream c i in (i, s, get_codec s)

let find_best_audio_stream c = find_best_stream c MT_audio
let find_best_video_stream c = find_best_stream c MT_video
let find_best_subtitle_stream c = find_best_stream c MT_subtitle

let get_input s = s.container
let get_index s = s.index

let get_duration ?(format=`Second) s = _get_duration s.container s.index format
let get_metadata s = List.rev(_get_metadata s.container s.index)

external select : (input, _)stream -> unit = "ocaml_av_select_stream"

external read_packet : (input, 'm)stream -> 'm Avcodec.Packet.t = "ocaml_av_read_stream_packet"

let rec iter_packet f stream =
  try
    f (read_packet stream);
    iter_packet f stream
  with
    | Error `Eof -> ()

external read_frame : (input, 'm)stream -> 'm frame = "ocaml_av_read_stream_frame"

let rec iter_frame f s =
  try
    f (read_frame s);
    iter_frame f s
  with
    | Error `Eof -> ()

type input_packet_result = [
  | `Audio of int * audio Avcodec.Packet.t
  | `Video of int * video Avcodec.Packet.t
  | `Subtitle of int * subtitle Avcodec.Packet.t
]

(** Reads the selected streams if any or all streams otherwise. *)
external read_input_packet : input container -> input_packet_result = "ocaml_av_read_input_packet"

(** Reads iteratively the selected streams if any or all streams otherwise. *)
let iter_input_packet ?(audio=(fun _ _->())) ?(video=(fun _ _->())) ?(subtitle=(fun _ _->())) src =
  let rec iter() = match read_input_packet src with
    | `Audio(index, packet) -> audio index packet; iter()
    | `Video(index, packet) -> video index packet; iter()
    | `Subtitle(index, packet) -> subtitle index packet; iter()
  in
  try
    iter()
  with Error `Eof -> ()


type input_frame_result = [
  | `Audio of int * audio frame
  | `Video of int * video frame
  | `Subtitle of int * subtitle frame
]

(** Reads the selected streams if any or all streams otherwise. *)
external read_input_frame : input container -> input_frame_result = "ocaml_av_read_input_frame"

(** Reads iteratively the selected streams if any or all streams otherwise. *)
let iter_input_frame ?(audio=(fun _ _->())) ?(video=(fun _ _->())) ?(subtitle=(fun _ _->())) src =
  let rec iter() = match read_input_frame src with
    | `Audio(index, frame) -> audio index frame; iter()
    | `Video(index, frame) -> video index frame; iter()
    | `Subtitle(index, frame) -> subtitle index frame; iter()
  in
  try
    iter()
  with Error `Eof -> ()


type seek_flag = Seek_flag_backward | Seek_flag_byte | Seek_flag_any | Seek_flag_frame

external seek : (input, _)stream -> Time_format.t -> Int64.t -> seek_flag array -> unit = "ocaml_av_seek_frame"

external reuse_output : input container -> bool -> unit = "ocaml_av_reuse_output"


(* Output *)
external open_output : string -> output container = "ocaml_av_open_output"

external ocaml_av_open_output_stream : (output, _) format -> avio -> output container = "ocaml_av_open_output_stream"

type write_callbacks = {
  write : write;
  seek : seek option
}

let open_output_stream format {write;seek} =
  let avio = ocaml_av_create_io 4096 None (Some write) (_seek_of_seek seek) in
  let cleanup () = caml_av_input_io_finalise avio in
  let output =
    ocaml_av_open_output_stream format avio
  in
  Gc.finalise_last cleanup output;
  output

external _set_metadata : output container -> int -> (string * string) array -> unit = "ocaml_av_set_metadata"
let set_output_metadata o tags = _set_metadata o (-1) (Array.of_list tags)

let set_metadata s tags = _set_metadata s.container s.index (Array.of_list tags)

let get_output s = s.container


external new_audio_stream : output container -> Avcodec.Audio.id -> Channel_layout.t -> Sample_format.t -> int -> int -> Avutil.rational -> int = "ocaml_av_new_audio_stream_byte" "ocaml_av_new_audio_stream"

let new_audio_stream ?codec_id ?codec_name ?channel_layout ?sample_format ?bit_rate ?sample_rate ?codec ?time_base ?stream o =

  let codec = match codec with
    | Some _ -> codec
    | None -> match stream with
      | Some stm -> Some(get_codec stm)
      | None -> codec
  in
  let ci = match codec_id with
    | Some ci -> ci
    | None -> match codec_name with
      | Some cn -> Avcodec.Audio.find_id cn
      | None -> match codec with
        | Some cp -> Avcodec.Audio.get_id cp
        | None -> raise (Error (`Failure "Audio codec undefined"))
  in
  let cl = match channel_layout with
    | Some cl -> cl
    | None -> match codec with
      | Some cp -> Avcodec.Audio.get_channel_layout cp
      | None -> `Stereo
  in
  let sf = match sample_format with
    | Some sf -> sf
    | None -> match codec with
      | Some cp -> Avcodec.Audio.get_sample_format cp
      | None -> Avcodec.Audio.find_best_sample_format ci `Dbl
  in
  let br = match bit_rate with
    | Some br -> br
    | None -> match codec with
      | Some cp -> Avcodec.Audio.get_bit_rate cp
      | None -> 192000
  in
  let sr = match sample_rate with
    | Some sr -> sr
    | None -> match codec with
      | Some cp -> Avcodec.Audio.get_sample_rate cp
      | None -> 44100
  in
  let tb = match time_base with
    | Some tb -> tb
    | None -> match stream with
      | Some stm -> get_time_base stm
      | None -> {num = 1; den = sr}
  in
  mk_stream o (new_audio_stream o ci cl sf br sr tb)


external new_video_stream : output container -> Avcodec.Video.id -> int -> int -> Pixel_format.t -> int -> int -> Avutil.rational -> int = "ocaml_av_new_video_stream_byte" "ocaml_av_new_video_stream"

let new_video_stream ?codec_id ?codec_name ?width ?height ?pixel_format ?bit_rate ?(frame_rate=25) ?codec ?time_base ?stream o =

  let codec = match codec with
    | Some _ -> codec
    | None -> match stream with
      | Some stm -> Some(get_codec stm)
      | None -> codec
  in
  let ci = match codec_id with
    | Some ci -> ci
    | None -> match codec_name with
      | Some cn -> Avcodec.Video.find_id cn
      | None -> match codec with
        | Some cp -> Avcodec.Video.get_id cp
        | None -> raise (Error (`Failure "Video codec undefined"))
  in
  let w = match width with
    | Some w -> w
    | None -> match codec with
      | Some cp -> Avcodec.Video.get_width cp
      | None -> raise (Error (`Failure "Video width undefined"))
  in
  let h = match height with
    | Some h -> h
    | None -> match codec with
      | Some cp -> Avcodec.Video.get_height cp
      | None -> raise (Error (`Failure "Video height undefined"))
  in
  let pf = match pixel_format with
    | Some pf -> pf
    | None -> match codec with
      | Some cp -> Avcodec.Video.get_pixel_format cp
      | None -> Avcodec.Video.find_best_pixel_format ci `Yuv420p
  in
  let br = match bit_rate with
    | Some br -> br
    | None -> match codec with
      | Some cp -> Avcodec.Video.get_bit_rate cp
      | None -> w * h * 4
  in
  let tb = match time_base with
    | Some tb -> tb
    | None -> match stream with
      | Some stm -> get_time_base stm
      | None -> {num = 1; den = frame_rate}
  in
  mk_stream o (new_video_stream o ci w h pf br frame_rate tb)


external new_subtitle_stream : output container -> Avcodec.Subtitle.id -> Avutil.rational -> int = "ocaml_av_new_subtitle_stream"

let new_subtitle_stream ?codec_id ?codec_name ?codec ?time_base ?stream o =

  let codec = match codec with
    | Some _ -> codec
    | None -> match stream with
      | Some stm -> Some(get_codec stm)
      | None -> codec
  in
  let ci = match codec_id with
    | Some ci -> ci
    | None -> match codec_name with
      | Some cn -> Avcodec.Subtitle.find_id cn
      | None -> match codec with
        | Some cp -> Avcodec.Subtitle.get_id cp
        | None -> raise (Error (`Failure "Subtitle codec undefined"))
  in
  let tb = match time_base with
    | Some tb -> tb
    | None -> match stream with
      | Some stm -> get_time_base stm
      | None -> Subtitle.time_base()
  in
  mk_stream o (new_subtitle_stream o ci tb)


external write_packet : (output, 'media)stream -> 'media Avcodec.Packet.t -> unit = "ocaml_av_write_stream_packet"

external write_frame : (output, 'media)stream -> 'media frame -> unit = "ocaml_av_write_stream_frame"

external write_audio_frame : output container -> audio frame -> unit = "ocaml_av_write_audio_frame"
external write_video_frame : output container -> video frame -> unit = "ocaml_av_write_video_frame"


external close : _ container -> unit = "ocaml_av_close"

