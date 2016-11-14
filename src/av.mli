
type t
type time_format = Avutil.Time_format.t
type pixel_format = Avutil.Pixel_format.t
type channel_layout = Avutil.Channel_layout.t
type sample_format = Avutil.Sample_format.t
type audio_frame = Avutil.Audio_frame.t
type video_frame = Avutil.Video_frame.t
type subtitle_frame = Avutil.Subtitle_frame.t


(** Open an input. *)
val open_input : string -> t

(** Close an input. *)
val close_input : t -> unit

(** Input tag list. *)
val get_metadata : t -> (string * string) list

(** Selected audio stream index. *)
val get_audio_stream_index : t -> int

(** Selected video stream index. *)
val get_video_stream_index : t -> int

(** Duration of an input stream. *)
val get_duration : t -> int -> time_format -> Int64.t


(** Selected audio stream channel layout. *)
val get_channel_layout : t -> channel_layout

(** Selected audio stream channels number. *)
val get_nb_channels : t -> int

(** Selected audio stream sample rate. *)
val get_sample_rate : t -> int

(** Selected audio stream sample format. *)
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

(** Read selected audio stream. *)
val read_audio : t -> audio

(** Read selected video stream. *)
val read_video : t -> video

(** Read selected subtitle stream. *)
val read_subtitle : t -> subtitle

(** Read input  streams. *)
val read : t -> media


(** Seek mode. *)
type seek_flag = Seek_flag_backward | Seek_flag_byte | Seek_flag_any | Seek_flag_frame

(** Seek in a stream. *)
val seek_frame : t -> int -> time_format -> Int64.t -> seek_flag array -> unit


(** Convert subtitle frame to string. *)
val subtitle_to_string : subtitle_frame -> string

