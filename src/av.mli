exception Open_failure of string

type t
type time_format = Avutil.Time_format.t
type pixel_format = Avutil.Pixel_format.t
type channel_layout = Avutil.Channel_layout.t
type sample_format = Avutil.Sample_format.t
type audio_frame = Avutil.Audio_frame.t
type video_frame = Avutil.Video_frame.t
type subtitle_frame = Avutil.Subtitle_frame.t


val open_input : string -> t

val get_metadata : t -> (string * string) list

val get_audio_stream_index : t -> int
val get_video_stream_index : t -> int

val get_duration : t -> int -> time_format -> Int64.t


val get_channel_layout : t -> channel_layout
val get_nb_channels : t -> int
val get_sample_rate : t -> int
val get_sample_format : t -> sample_format

val get_audio_format : t -> Avutil.audio_format

type audio = Audio of audio_frame | End_of_file
type video = Video of video_frame | End_of_file
type subtitle = Subtitle of subtitle_frame | End_of_file

type media =
    Audio of int * audio_frame
  | Video of int * video_frame
  | Subtitle of int * subtitle_frame
  | End_of_file

val read_audio : t -> audio
val read_video : t -> video
val read_subtitle : t -> subtitle
val read : t -> media


type seek_flag = Seek_flag_backward | Seek_flag_byte | Seek_flag_any | Seek_flag_frame

val seek_frame : t -> int -> time_format -> Int64.t -> seek_flag array -> unit


val subtitle_to_string : subtitle_frame -> string

