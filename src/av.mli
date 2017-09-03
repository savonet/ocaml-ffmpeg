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


module Input_format : sig
  type t
  val get_name : t -> string
  val get_long_name : t -> string
end


type input_t

(** Open an input URL. *)
val open_input : string -> input_t

(** Open an input format. *)
val open_input_format : Input_format.t -> input_t

(** Close an input. *)
val close_input : input_t -> unit

(** Input tag list. *)
val get_metadata : input_t -> (string * string) list

(** Selected audio stream index. *)
val get_audio_stream_index : input_t -> int

(** Selected video stream index. *)
val get_video_stream_index : input_t -> int

(** Duration of an input stream. *)
val get_duration : input_t -> int -> time_format -> Int64.t


(** Selected audio stream channel layout. *)
val get_channel_layout : input_t -> channel_layout

(** Selected audio stream channels number. *)
val get_nb_channels : input_t -> int

(** Selected audio stream sample rate. *)
val get_sample_rate : input_t -> int

(** Selected audio stream sample format. *)
val get_sample_format : input_t -> sample_format

val get_audio_format : input_t -> Avutil.audio_format

(** Best audio stream codec parameters. *)
val get_input_audio_codec_parameters : input_t -> Avcodec.Audio.Parameters.t


type audio = Audio of audio_frame | End_of_file
type video = Video of video_frame | End_of_file
type subtitle = Subtitle of subtitle_frame | End_of_file

type media =
  | Audio of int * audio_frame
  | Video of int * video_frame
  | Subtitle of int * subtitle_frame
  | End_of_file

(** Read selected audio stream. *)
val read_audio : input_t -> audio

(** Read iteratively selected audio stream. *)
val iter_audio : (audio_frame -> unit) -> input_t -> unit

(** Read selected video stream. *)
val read_video : input_t -> video

(** Read iteratively selected video stream. *)
val iter_video : (video_frame -> unit) -> input_t -> unit

(** Read selected subtitle stream. *)
val read_subtitle : input_t -> subtitle

(** Read iteratively selected subtitle stream. *)
val iter_subtitle : (subtitle_frame -> unit) -> input_t -> unit

(** Read input  streams. *)
val read : input_t -> media


(** Seek mode. *)
type seek_flag = Seek_flag_backward | Seek_flag_byte | Seek_flag_any | Seek_flag_frame

(** Seek in a stream. *)
val seek_frame : input_t -> int -> time_format -> Int64.t -> seek_flag array -> unit


(** Convert subtitle frame to string. *)
val subtitle_to_string : subtitle_frame -> string



module Output_format : sig
  type t
  val get_name : t -> string
  val get_long_name : t -> string
  val get_audio_codec : t -> audio_codec_id
  val get_video_codec : t -> video_codec_id
end


type output_t

(** Open an output file. *)
val open_output : ?audio_parameters:Avcodec.Audio.Parameters.t -> string -> output_t

(** Open an output format. *)
val open_output_format : ?audio_parameters:Avcodec.Audio.Parameters.t -> Output_format.t -> output_t

(** Open an output format by name. *)
val open_output_format_name : ?audio_parameters:Avcodec.Audio.Parameters.t -> string -> output_t

(** Close an output. *)
val close_output : output_t -> unit

(** Best audio stream codec parameters. *)
val get_output_audio_codec_parameters : output_t -> Avcodec.Audio.Parameters.t
                                                      
(** Add a new audio stream *)
val new_audio_stream : ?codec_id:audio_codec_id -> ?codec_name:string -> ?channel_layout:channel_layout -> ?sample_format:sample_format -> ?bit_rate:int -> ?sample_rate:int -> ?codec_parameters:Avcodec.Audio.Parameters.t -> output_t -> int

(** Add a new video stream *)
val new_video_stream : ?codec_id:video_codec_id -> ?codec_name:string -> int -> int -> pixel_format -> ?bit_rate:int -> ?frame_rate:int -> output_t -> int


(** Write an audio frame to a stream *)
val write_audio_frame : output_t -> int -> audio_frame -> unit


(** Write an video frame to a stream *)
val write_video_frame : output_t -> int -> video_frame -> unit
