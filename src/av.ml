(*exception End_of_file*)
exception Open_failure of string

let () =
  Callback.register_exception "ffmpeg_exn_open_failure" (Open_failure "")

type t

external open_input : string -> t = "ocaml_av_open_input"

external get_metadata : t -> (string * string) list = "ocaml_av_get_metadata"

type pixel_format = Avutil.Pixel_format.t
type channel_layout = Avutil.Channel_layout.t
type sample_format = Avutil.Sample_format.t

external get_channel_layout : t -> channel_layout = "ocaml_av_get_channel_layout"
external get_nb_channels : t -> int = "ocaml_av_get_nb_channels"
external get_sample_rate : t -> int = "ocaml_av_get_sample_rate"
external get_sample_format : t -> sample_format = "ocaml_av_get_sample_format"

let get_audio_format t = Avutil.{
  channel_layout = get_channel_layout t;
  sample_format = get_sample_format t;
  sample_rate = get_sample_rate t
}

type audio_frame = Avutil.Audio_frame.t
type video_frame = Avutil.Video_frame.t
type subtitle_frame = Avutil.Subtitle_frame.t

type audio =
  | Audio of audio_frame
  | End_of_file

type video =
  | Video of video_frame
  | End_of_file

type subtitle =
  | Subtitle of subtitle_frame
  | End_of_file

type media =
  | Audio of int * audio_frame
  | Video of int * video_frame
  | Subtitle of int * subtitle_frame
  | End_of_file

external read_audio : t -> audio = "ocaml_av_read_audio"
external read_video : t -> video = "ocaml_av_read_video"
external read_subtitle : t -> subtitle = "ocaml_av_read_subtitle"
external read : t -> media = "ocaml_av_read"


type seek_flag = Seek_flag_backward | Seek_flag_byte | Seek_flag_any | Seek_flag_frame

external seek_frame : t -> int -> Int64.t -> seek_flag array -> unit = "ocaml_av_seek_frame"


external subtitle_to_string : subtitle_frame -> string = "ocaml_av_subtitle_to_string"
