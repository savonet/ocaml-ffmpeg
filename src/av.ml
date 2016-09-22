exception End_of_file
exception Failure
exception Failure_msg of string
exception Open_failure of string

let () =
  Callback.register_exception "ffmpeg_exn_end_of_file" End_of_file;
  Callback.register_exception "ffmpeg_exn_failure" Failure;
  Callback.register_exception "ffmpeg_exn_failure_msg" (Failure_msg "");
  Callback.register_exception "ffmpeg_exn_open_failure" (Open_failure "")

type t

external open_input : string -> t = "ocaml_av_open_input"

external get_metadata : t -> (string * string) list = "ocaml_av_get_metadata"

type pixel_format = Avutil.Pixel_format.t

type channel_layout = Avutil.Channel_layout.t

type sample_format = Avutil.Sample_format.t

external get_audio_channel_layout : t -> channel_layout = "ocaml_av_get_audio_channel_layout"
external get_audio_nb_channels : t -> int = "ocaml_av_get_audio_nb_channels"
external get_audio_sample_rate : t -> int = "ocaml_av_get_audio_sample_rate"
external get_audio_sample_format : t -> sample_format = "ocaml_av_get_audio_sample_format"

external create_resample : ?channel_layout:channel_layout -> ?sample_format:sample_format -> ?sample_rate:int -> t -> unit = "ocaml_av_create_resample"

type audio_frame = Avutil.Audio_frame.t
type video_frame = Avutil.Video_frame.t
type subtitle_frame = Avutil.Subtitle_frame.t

type media =
    Audio of int * audio_frame
  | Video of int * video_frame
  | Subtitle of int * subtitle_frame

external read_audio : t -> audio_frame = "ocaml_av_read_audio"
external read_video : t -> video_frame = "ocaml_av_read_video"
external read_subtitle : t -> subtitle_frame = "ocaml_av_read_subtitle"
external read : t -> media = "ocaml_av_read"



external video_to_string : video_t -> string = "ocaml_av_video_to_string"
external subtitle_to_string : subtitle_t -> string = "ocaml_av_subtitle_to_string"
