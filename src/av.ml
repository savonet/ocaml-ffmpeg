type t
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


external open_input : string -> t = "ocaml_av_open_input"
external close_input : t -> unit = "ocaml_av_close_input"

external get_metadata : t -> (string * string) list = "ocaml_av_get_metadata"

external get_audio_stream_index : t -> int = "ocaml_av_get_audio_stream_index"
external get_video_stream_index : t -> int = "ocaml_av_get_video_stream_index"

external get_duration : t -> int -> time_format -> Int64.t = "ocaml_av_get_duration"

external get_channel_layout : t -> channel_layout = "ocaml_av_get_channel_layout"
external get_nb_channels : t -> int = "ocaml_av_get_nb_channels"
external get_sample_rate : t -> int = "ocaml_av_get_sample_rate"
external get_sample_format : t -> sample_format = "ocaml_av_get_sample_format"

let get_audio_format t =
  {
    Avutil.channel_layout = get_channel_layout t;
    Avutil.sample_format = get_sample_format t;
    Avutil.sample_rate = get_sample_rate t
  }


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


type seek_flag = Seek_flag_backward | Seek_flag_byte | Seek_flag_any | Seek_flag_frame

external seek_frame : t -> int -> time_format -> Int64.t -> seek_flag array -> unit = "ocaml_av_seek_frame"

external subtitle_to_string : subtitle_frame -> string = "ocaml_av_subtitle_to_string"


external open_output : string -> t = "ocaml_av_open_output"
external close_output : t -> unit = "ocaml_av_close_output"

external new_audio_stream : t -> audio_codec_id -> channel_layout -> sample_format -> int -> int -> int = "ocaml_av_new_audio_stream_byte" "ocaml_av_new_audio_stream"

let new_audio_stream ?codec_id ?codec_name cl ?sample_format ?(bit_rate=192000) sr t =

  let ci = match codec_id with
    | Some ci -> ci
    | None -> match codec_name with
      | Some cn -> Avcodec.Audio.find_by_name cn
      | None -> raise(Failure "Audio codec undefined")
  in
  let sf = match sample_format with
    | Some sf -> sf
    | None -> Avcodec.Audio.find_best_sample_format ci
  in
  new_audio_stream t ci cl sf bit_rate sr


external new_video_stream : t -> video_codec_id -> int -> int -> pixel_format -> int -> int -> int = "ocaml_av_new_video_stream_byte" "ocaml_av_new_video_stream"

let new_video_stream ?codec_id ?codec_name w h pixel_format ?(bit_rate=1000000) ?(frame_rate=25) t =

  let ci = match codec_id with
    | Some ci -> ci
    | None -> match codec_name with
      | Some cn -> Avcodec.Video.find_by_name cn
      | None -> raise(Failure "Video codec undefined")
  in
  new_video_stream t ci w h pixel_format bit_rate frame_rate


external write_audio_frame : t -> int -> audio_frame -> unit = "ocaml_av_write_audio_frame"

external write_video_frame : t -> int -> video_frame -> unit = "ocaml_av_write_video_frame"
