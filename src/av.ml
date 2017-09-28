type time_format = Avutil.Time_format.t
type pixel_format = Avutil.Pixel_format.t
type channel_layout = Avutil.Channel_layout.t
type sample_format = Avutil.Sample_format.t
type audio_frame = Avutil.Audio_frame.t
type video_frame = Avutil.Video_frame.t
type subtitle_frame = Avutil.Subtitle_frame.t
type audio_codec_id = Avcodec.Audio.id
type video_codec_id = Avcodec.Video.id
type subtitle_codec_id = Avcodec.Subtitle.id


type t

external get_audio_codec_parameters : t -> Avcodec.Audio.Parameters.t = "ocaml_av_get_audio_codec_parameters"

external get_video_codec_parameters : t -> Avcodec.Video.Parameters.t = "ocaml_av_get_video_codec_parameters"

external get_streams_codec_parameters : t -> Avcodec.Parameters.t array = "ocaml_av_get_streams_codec_parameters"


(* Input *)
module Input = struct
  type t

  module Format = struct
    type t
    external get_name : t -> string = "ocaml_av_input_format_get_name"
    external get_long_name : t -> string = "ocaml_av_input_format_get_long_name"
  end

  external _open : string option -> Format.t option -> t = "ocaml_av_open_input"
  let open_url url = _open (Some url) None
  let open_format format = _open None (Some format)

  external close : t -> unit = "ocaml_av_close_input"

  external get_metadata : t -> (string * string) list = "ocaml_av_get_metadata"

  external get_duration : t -> int -> time_format -> Int64.t = "ocaml_av_get_duration"


  type audio =
    | Audio of audio_frame
    | End_of_file

  external read_audio : t -> audio = "ocaml_av_read_audio"

  let iter_audio f src =
    let rec iter() = match read_audio src with
      | Audio frame -> f frame; iter()
      | End_of_file -> ()
    in
    iter();


  type video =
    | Video of video_frame
    | End_of_file

  external read_video : t -> video = "ocaml_av_read_video"

  let iter_video f src =
    let rec iter() = match read_video src with
      | Video frame -> f frame; iter()
      | End_of_file -> ()
    in
    iter();


  type subtitle =
    | Subtitle of subtitle_frame
    | End_of_file

  external read_subtitle : t -> subtitle = "ocaml_av_read_subtitle"

  let iter_subtitle f src =
    let rec iter() = match read_subtitle src with
      | Subtitle frame -> f frame; iter()
      | End_of_file -> ()
    in
    iter();


  type media =
    | Audio of int * audio_frame
    | Video of int * video_frame
    | Subtitle of int * subtitle_frame
    | End_of_file

  external read : t -> media = "ocaml_av_read"

  let iter ?(audio=(fun _ _ -> ())) ?(video=(fun _ _ -> ())) ?(subtitle=(fun _ _ -> ())) src =
    let rec iter() = match read src with
      | Audio (idx, frame) -> audio idx frame; iter()
      | Video (idx, frame) -> video idx frame; iter()
      | Subtitle (idx, frame) -> subtitle idx frame; iter()
      | End_of_file -> ()
    in
    iter()


  type seek_flag = Seek_flag_backward | Seek_flag_byte | Seek_flag_any | Seek_flag_frame

  external seek_frame : t -> int -> time_format -> Int64.t -> seek_flag array -> unit = "ocaml_av_seek_frame"
end

external of_input : Input.t -> t = "%identity"
let (>-) i f = f(of_input i)

(* Output *)
module Output = struct
  type t

  module Format = struct
    type t
    external get_name : t -> string = "ocaml_av_output_format_get_name"
    external get_long_name : t -> string = "ocaml_av_output_format_get_long_name"
    external get_audio_codec : t -> audio_codec_id = "ocaml_av_output_format_get_audio_codec"
    external get_video_codec : t -> video_codec_id = "ocaml_av_output_format_get_video_codec"
  end

  external _open : string option -> Format.t option -> string option -> t = "ocaml_av_open_output"

  let open_file filename = _open (Some filename) None None

  let open_format format = _open None (Some format) None

  let open_format_name format_name = _open None None (Some format_name)

  external close : t -> unit = "ocaml_av_close_output"


  external set_metadata : t -> (string * string) array -> unit = "ocaml_av_set_metadata"
  let set_metadata t tags = set_metadata t (Array.of_list tags)

  
  external new_audio_stream : t -> audio_codec_id -> channel_layout -> sample_format -> int -> int -> int = "ocaml_av_new_audio_stream_byte" "ocaml_av_new_audio_stream"

  let new_audio_stream ?codec_id ?codec_name ?channel_layout ?sample_format ?bit_rate ?sample_rate ?codec_parameters t =

    let ci = match codec_id with
      | Some ci -> ci
      | None -> match codec_name with
        | Some cn -> Avcodec.Audio.find_by_name cn
        | None -> match codec_parameters with
          | Some cp -> Avcodec.Audio.Parameters.get_codec_id cp
          | None -> raise(Failure "Audio codec undefined")
    in
    let cl = match channel_layout with
      | Some cl -> cl
      | None -> match codec_parameters with
        | Some cp -> Avcodec.Audio.Parameters.get_channel_layout cp
        | None -> Avutil.Channel_layout.CL_stereo
    in
    let sf = match sample_format with
      | Some sf -> sf
      | None -> match codec_parameters with
        | Some cp -> Avcodec.Audio.Parameters.get_sample_format cp
        | None -> Avcodec.Audio.find_best_sample_format ci
    in
    let br = match bit_rate with
      | Some br -> br
      | None -> match codec_parameters with
        | Some cp -> Avcodec.Audio.Parameters.get_bit_rate cp
        | None -> 192000
    in
    let sr = match sample_rate with
      | Some sr -> sr
      | None -> match codec_parameters with
        | Some cp -> Avcodec.Audio.Parameters.get_sample_rate cp
        | None -> 44100
    in
    new_audio_stream t ci cl sf br sr


  external new_video_stream : t -> video_codec_id -> int -> int -> pixel_format -> int -> int -> int = "ocaml_av_new_video_stream_byte" "ocaml_av_new_video_stream"

  let new_video_stream ?codec_id ?codec_name ?width ?height ?pixel_format ?bit_rate ?(frame_rate=25) ?codec_parameters t =

    let ci = match codec_id with
      | Some ci -> ci
      | None -> match codec_name with
        | Some cn -> Avcodec.Video.find_by_name cn
        | None -> match codec_parameters with
          | Some cp -> Avcodec.Video.Parameters.get_codec_id cp
          | None -> raise(Failure "Video codec undefined")
    in
    let w = match width with
      | Some w -> w
      | None -> match codec_parameters with
        | Some cp -> Avcodec.Video.Parameters.get_width cp
        | None -> raise(Failure "Video width undefined")
    in
    let h = match height with
      | Some h -> h
      | None -> match codec_parameters with
        | Some cp -> Avcodec.Video.Parameters.get_height cp
        | None -> raise(Failure "Video height undefined")
    in
    let pf = match pixel_format with
      | Some pf -> pf
      | None -> match codec_parameters with
        | Some cp -> Avcodec.Video.Parameters.get_pixel_format cp
        | None -> Avcodec.Video.find_best_pixel_format ci
    in
    let br = match bit_rate with
      | Some br -> br
      | None -> match codec_parameters with
        | Some cp -> Avcodec.Video.Parameters.get_bit_rate cp
        | None -> w * h * 4
    in
    new_video_stream t ci w h pf br frame_rate


  external new_subtitle_stream : t -> subtitle_codec_id -> int = "ocaml_av_new_subtitle_stream"

  let new_subtitle_stream ?codec_id ?codec_name ?codec_parameters t =

    let ci = match codec_id with
      | Some ci -> ci
      | None -> match codec_name with
        | Some cn -> Avcodec.Subtitle.find_by_name cn
        | None -> match codec_parameters with
          | Some cp -> Avcodec.Subtitle.Parameters.get_codec_id cp
          | None -> raise(Failure "Subtitle codec undefined")
    in
    new_subtitle_stream t ci


  external write_audio_frame : t -> int -> audio_frame -> unit = "ocaml_av_write_audio_frame"

  external write_video_frame : t -> int -> video_frame -> unit = "ocaml_av_write_video_frame"

  external write_subtitle_frame : t -> int -> subtitle_frame -> unit = "ocaml_av_write_subtitle_frame"
end

external of_output : Output.t -> t = "%identity"
let (-<) o f = f(of_output o)
