open Avutil

(* Format *)
module Format = struct
  external get_input_name : (input, _)format -> string = "ocaml_av_input_format_get_name"
  external get_input_long_name : (input, _)format -> string = "ocaml_av_input_format_get_long_name"

  external get_output_name : (output, _)format -> string = "ocaml_av_output_format_get_name"
  external get_output_long_name : (output, _)format -> string = "ocaml_av_output_format_get_long_name"
  external get_audio_codec_id : (output, audio)format -> Avcodec.Audio.id = "ocaml_av_output_format_get_audio_codec_id"
  external get_video_codec_id : (output, video)format -> Avcodec.Video.id = "ocaml_av_output_format_get_video_codec_id"
  external get_subtitle_codec_id : (output, subtitle)format -> Avcodec.Subtitle.id = "ocaml_av_output_format_get_subtitle_codec_id"
end




(* Input *)
external _open_input : string option -> (input, _)format option -> input container = "ocaml_av_open_input"
let open_input url = _open_input (Some url) None
let open_input_format format = _open_input None (Some format)

external _get_duration : input container -> int -> Time_format.t -> Int64.t = "ocaml_av_get_duration"
let get_input_duration i fmt = _get_duration i (-1) fmt

external _get_metadata : input container -> int -> (string * string) list = "ocaml_av_get_metadata"
let get_input_metadata i = List.rev(_get_metadata i (-1))


(* Input Stream *)
type ('a, 'b) stream = {container : 'a container; index : int}

type media_type = MT_audio | MT_video | MT_subtitle


let mk_stream container index = {container; index}

external get_codec : (_, 'm)stream -> 'm Avcodec.t = "ocaml_av_get_stream_codec_parameters"


external _get_streams : input container -> media_type -> int array = "ocaml_av_get_streams"

let get_streams input media_type =
  _get_streams input media_type
  |> Array.map(fun i -> let s = mk_stream input i in (i, s, get_codec s))
  |> Array.to_list

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

let get_duration s fmt = _get_duration s.container s.index fmt
let get_metadata s = List.rev(_get_metadata s.container s.index)

external select : (input, _)stream -> unit = "ocaml_av_select_stream"

type 'a result = Frame of 'a frame | End_of_stream


external read : (input, 'm)stream -> 'm result = "ocaml_av_read_stream"

let iter f s =
  let rec iter() = match read s with
    | Frame frame -> f frame; iter()
    | End_of_stream -> ()
  in
  iter()


type seek_flag = Seek_flag_backward | Seek_flag_byte | Seek_flag_any | Seek_flag_frame

external seek : (input, _)stream -> Time_format.t -> Int64.t -> seek_flag array -> unit = "ocaml_av_seek_frame"

type media =
  | Audio of int * audio frame
  | Video of int * video frame
  | Subtitle of int * subtitle frame
  | End_of_file

(** Reads the selected streams if any or all streams otherwise. *)
external read_input : input container -> media = "ocaml_av_read_input"

(** Reads iteratively the selected streams if any or all streams otherwise. *)
let iter_input ?(audio=(fun _ _->())) ?(video=(fun _ _->())) ?(subtitle=(fun _ _->())) src =
  let rec iter() = match read_input src with
    | Audio(index, frame) -> audio index frame; iter()
    | Video(index, frame) -> video index frame; iter()
    | Subtitle(index, frame) -> subtitle index frame; iter()
    | End_of_file -> ()
  in
  iter()


(* Output *)
external _open_output : string option -> (output, _)format option -> string option -> output container = "ocaml_av_open_output"

let open_output filename = _open_output (Some filename) None None
let open_output_format format = _open_output None (Some format) None
let open_output_format_name format_name = _open_output None None (Some format_name)


external _set_metadata : output container -> int -> (string * string) array -> unit = "ocaml_av_set_metadata"
let set_output_metadata o tags = _set_metadata o (-1) (Array.of_list tags)

let set_metadata s tags = _set_metadata s.container s.index (Array.of_list tags)

let get_output s = s.container

external new_audio_stream : output container -> Avcodec.Audio.id -> Channel_layout.t -> Sample_format.t -> int -> int -> int = "ocaml_av_new_audio_stream_byte" "ocaml_av_new_audio_stream"

let new_audio_stream ?codec_id ?codec_name ?channel_layout ?sample_format ?bit_rate ?sample_rate ?codec o =

  let ci = match codec_id with
    | Some ci -> ci
    | None -> match codec_name with
      | Some cn -> Avcodec.Audio.find_id cn
      | None -> match codec with
        | Some cp -> Avcodec.Audio.get_id cp
        | None -> raise(Failure "Audio codec undefined")
  in
  let cl = match channel_layout with
    | Some cl -> cl
    | None -> match codec with
      | Some cp -> Avcodec.Audio.get_channel_layout cp
      | None -> Channel_layout.CL_stereo
  in
  let sf = match sample_format with
    | Some sf -> sf
    | None -> match codec with
      | Some cp -> Avcodec.Audio.get_sample_format cp
      | None -> Avcodec.Audio.find_best_sample_format ci
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
  mk_stream o (new_audio_stream o ci cl sf br sr)


external new_video_stream : output container -> Avcodec.Video.id -> int -> int -> Pixel_format.t -> int -> int -> int = "ocaml_av_new_video_stream_byte" "ocaml_av_new_video_stream"

let new_video_stream ?codec_id ?codec_name ?width ?height ?pixel_format ?bit_rate ?(frame_rate=25) ?codec o =

  let ci = match codec_id with
    | Some ci -> ci
    | None -> match codec_name with
      | Some cn -> Avcodec.Video.find_id cn
      | None -> match codec with
        | Some cp -> Avcodec.Video.get_id cp
        | None -> raise(Failure "Video codec undefined")
  in
  let w = match width with
    | Some w -> w
    | None -> match codec with
      | Some cp -> Avcodec.Video.get_width cp
      | None -> raise(Failure "Video width undefined")
  in
  let h = match height with
    | Some h -> h
    | None -> match codec with
      | Some cp -> Avcodec.Video.get_height cp
      | None -> raise(Failure "Video height undefined")
  in
  let pf = match pixel_format with
    | Some pf -> pf
    | None -> match codec with
      | Some cp -> Avcodec.Video.get_pixel_format cp
      | None -> Avcodec.Video.find_best_pixel_format ci
  in
  let br = match bit_rate with
    | Some br -> br
    | None -> match codec with
      | Some cp -> Avcodec.Video.get_bit_rate cp
      | None -> w * h * 4
  in
  mk_stream o (new_video_stream o ci w h pf br frame_rate)


external new_subtitle_stream : output container -> Avcodec.Subtitle.id -> int = "ocaml_av_new_subtitle_stream"

let new_subtitle_stream ?codec_id ?codec_name ?codec o =

  let ci = match codec_id with
    | Some ci -> ci
    | None -> match codec_name with
      | Some cn -> Avcodec.Subtitle.find_id cn
      | None -> match codec with
        | Some cp -> Avcodec.Subtitle.get_id cp
        | None -> raise(Failure "Subtitle codec undefined")
  in
  mk_stream o (new_subtitle_stream o ci)


external write : (output, 'media)stream -> 'media frame -> unit = "ocaml_av_write_stream"

external write_output : output container -> _ frame -> media_type -> unit = "ocaml_av_write_output"

let write_audio oc af = write_output oc af MT_audio
let write_video oc af = write_output oc af MT_video


external close : _ container -> unit = "ocaml_av_close"

