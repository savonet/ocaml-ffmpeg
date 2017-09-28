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

(** Best audio stream codec parameters. *)
val get_audio_codec_parameters : t -> Avcodec.Audio.Parameters.t

(** Best video stream codec parameters. *)
val get_video_codec_parameters : t -> Avcodec.Video.Parameters.t

(** Streams codec parameters array. *)
val get_streams_codec_parameters : t -> Avcodec.Parameters.t array


(* Input *)
module Input : sig
  type t

  module Format : sig
    type t
    val get_name : t -> string
    val get_long_name : t -> string
  end


  (** Open an input URL. *)
  val open_url : string -> t

  (** Open an input format. *)
  val open_format : Format.t -> t

  (** Close an input. *)
  val close : t -> unit

  (** Input tag list. *)
  val get_metadata : t -> (string * string) list

  (** Duration of an input stream. *)
  val get_duration : t -> int -> time_format -> Int64.t


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

  (** Read input streams. *)
  val read : t -> media

  (** Read iteratively input streams. *)
  val iter : ?audio:(int -> audio_frame -> unit) ->
    ?video:(int -> video_frame -> unit) ->
    ?subtitle:(int -> subtitle_frame -> unit) ->
    t -> unit


  (** Seek mode. *)
  type seek_flag = Seek_flag_backward | Seek_flag_byte | Seek_flag_any | Seek_flag_frame

  (** Seek in a stream. *)
  val seek_frame : t -> int -> time_format -> Int64.t -> seek_flag array -> unit
end

val of_input : Input.t -> t
val (>-) : Input.t -> (t -> 'a) -> 'a

val of_input : Input.t -> t
val (>-) : Input.t -> (t -> 'a) -> 'a

module Output : sig
  type t

  module Format : sig
    type t
    val get_name : t -> string
    val get_long_name : t -> string
    val get_audio_codec : t -> audio_codec_id
    val get_video_codec : t -> video_codec_id
  end


  (** Open an output file. *)
  val open_file : string -> t

  (** Open an output format. *)
  val open_format : Format.t -> t

  (** Open an output format by name. *)
  val open_format_name : string -> t

  (** Close an output. *)
  val close : t -> unit

  (** Set output tag list. This must be set before starting writing streams. *)
  val set_metadata : t -> (string * string) list -> unit


  (** Add a new audio stream. This must be set before starting writing streams. *)
  val new_audio_stream : ?codec_id:audio_codec_id -> ?codec_name:string -> ?channel_layout:channel_layout -> ?sample_format:sample_format -> ?bit_rate:int -> ?sample_rate:int -> ?codec_parameters:Avcodec.Audio.Parameters.t -> t -> int

  (** Add a new video stream. This must be set before starting writing streams. *)
  val new_video_stream : ?codec_id:video_codec_id -> ?codec_name:string -> ?width:int -> ?height:int -> ?pixel_format:pixel_format -> ?bit_rate:int -> ?frame_rate:int -> ?codec_parameters:Avcodec.Video.Parameters.t -> t -> int

  (** Add a new subtitle stream. This must be set before starting writing streams. *)
  val new_subtitle_stream : ?codec_id:subtitle_codec_id -> ?codec_name:string -> ?codec_parameters:Avcodec.Subtitle.Parameters.t -> t -> int


  (** Write an audio frame to a stream *)
  val write_audio_frame : t -> int -> audio_frame -> unit

  (** Write an video frame to a stream *)
  val write_video_frame : t -> int -> video_frame -> unit

  (** Write an subtitle frame to a stream *)
  val write_subtitle_frame : t -> int -> subtitle_frame -> unit
end

val of_output : Output.t -> t
val (-<) : Output.t -> (t -> 'a) -> 'a
