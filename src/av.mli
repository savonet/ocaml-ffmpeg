
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
  | Audio of int * audio_frame
  | Video of int * video_frame
  | Subtitle of int * subtitle_frame
  | End_of_file

(** Read selected audio stream. *)
val read_audio : t -> audio

(** Read iteratively selected audio stream. *)
val iter_audio : (audio_frame -> unit) -> t -> unit

(** Read selected video stream. *)
val read_video : t -> video

(** Read iteratively selected video stream. *)
val iter_video : (video_frame -> unit) -> t -> unit

(** Read selected subtitle stream. *)
val read_subtitle : t -> subtitle

(** Read iteratively selected subtitle stream. *)
val iter_subtitle : (subtitle_frame -> unit) -> t -> unit

(** Read input  streams. *)
val read : t -> media


(** Seek mode. *)
type seek_flag = Seek_flag_backward | Seek_flag_byte | Seek_flag_any | Seek_flag_frame

(** Seek in a stream. *)
val seek_frame : t -> int -> time_format -> Int64.t -> seek_flag array -> unit


(** Convert subtitle frame to string. *)
val subtitle_to_string : subtitle_frame -> string


(** Open an output. *)
val open_output : string -> t

(** Close an output. *)
val close_output : t -> unit

(** Add a new audio stream *)
val new_audio_stream : ?codec_id:audio_codec_id -> ?codec_name:string -> channel_layout -> ?sample_format:sample_format -> ?bit_rate:int -> int -> t -> int

(** Add a new video stream *)
val new_video_stream : ?codec_id:video_codec_id -> ?codec_name:string -> int -> int -> pixel_format -> ?bit_rate:int -> ?frame_rate:int -> t -> int


(** Write an audio frame to a stream *)
val write_audio_frame : t -> int -> audio_frame -> unit


(** Write an video frame to a stream *)
val write_video_frame : t -> int -> video_frame -> unit
