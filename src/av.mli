exception End_of_file
exception Open_failure of string

type t

val open_input : string -> t
val get_metadata : t -> (string * string) list

type pixel_format = Avutil.Pixel_format.t
type channel_layout = Avutil.Channel_layout.t
type sample_format = Avutil.Sample_format.t

val get_channel_layout : t -> channel_layout
val get_nb_channels : t -> int
val get_sample_rate : t -> int
val get_sample_format : t -> sample_format

val get_audio_format : t -> Avutil.audio_format

(*
val create_resample :
  ?channel_layout:channel_layout ->
  ?sample_format:sample_format -> ?sample_rate:int -> t -> Swresample.t
*)
type audio_frame = Avutil.Audio_frame.t
type video_frame = Avutil.Video_frame.t
type subtitle_frame = Avutil.Subtitle_frame.t

type media =
    Audio of int * audio_frame
  | Video of int * video_frame
  | Subtitle of int * subtitle_frame

val read_audio : t -> audio_frame
val read_video : t -> video_frame
val read_subtitle : t -> subtitle_frame
val read : t -> media
val video_to_string : video_frame -> string
val subtitle_to_string : subtitle_frame -> string

