(** Input, output and streams reading and writing module *)

open Avutil

(** Format *)

(** Return the name of the format *)
val get_format_name : (_, _)format -> string

(** Return the long name of the format *)
val get_format_long_name : (_, _)format -> string

(** Return the audio codec id of the output audio format *)
val get_format_audio_codec_id : (output, audio)format -> Avcodec.Audio.id

(** Return the video codec id of the output video format *)
val get_format_video_codec_id : (output, video)format -> Avcodec.Video.id

(** Input *)

(** Open an input URL. *)
val open_input : string -> input container

(** Open an input format. *)
val open_input_format : (input, _)format -> input container

(** Close an input. *)
val close_input : input container -> unit


(** Return the duration of an input. *)
val get_input_duration : input container -> Time_format.t -> Int64.t

(** Return the input tag list. *)
val get_input_metadata : input container -> (string * string) list


(** Input/output, audio/video/subtitle stream type *)
type ('line, 'media) stream

(** Return the audio stream list of the input *)
val get_audio_streams : input container -> (int * (input, audio)stream * audio Avcodec.t) list

(** Return the video stream list of the input *)
val get_video_streams : input container -> (int * (input, video)stream * video Avcodec.t) list

(** Return the subtitle stream list of the input *)
val get_subtitle_streams : input container -> (int * (input, subtitle)stream * subtitle Avcodec.t) list


(** Return the best audio stream of the input *)
val find_best_audio_stream : input container -> (int * (input, audio)stream * audio Avcodec.t)

(** Return the best video stream of the input *)
val find_best_video_stream : input container -> (int * (input, video)stream * video Avcodec.t)

(** Return the best subtitle stream of the input *)
val find_best_subtitle_stream : input container -> (int * (input, subtitle)stream * subtitle Avcodec.t)

(** Return the index of the stream. *)
val get_index : (_, _)stream -> int

(** Return the codec of the stream. *)
val get_codec : (_, 'media)stream -> 'media Avcodec.t

(** Return the duration of the input stream. *)
val get_duration : (input, _)stream -> Time_format.t -> Int64.t

(** Return the input stream tag list. *)
val get_metadata : (input, _)stream -> (string * string) list

(** Select the input stream for reading. *)
val select : (input, _)stream -> unit

(** Stream reading result. *)
type 'media result = Frame of 'media frame | End_of_file

(** Read the input stream. *)
val read : (input, 'media)stream -> 'media result

(** Read iteratively the input stream. *)
val iter : ('media frame -> unit) -> (input, 'media)stream -> unit

(** Seek mode. *)
type seek_flag = Seek_flag_backward | Seek_flag_byte | Seek_flag_any | Seek_flag_frame

(** Seek in the input stream. *)
val seek : (input, _)stream -> Time_format.t -> Int64.t -> seek_flag array -> unit


type media =
  | Audio of int * audio frame
  | Video of int * video frame
  | Subtitle of int * subtitle frame
  | End_of_file

(** Reads the selected streams if any or all streams otherwise. *)
val read_input : input container -> media

(** Reads iteratively the selected streams if any or all streams otherwise. *)
val iter_input : ?audio:(int -> audio frame -> unit) ->
  ?video:(int -> video frame -> unit) ->
  ?subtitle:(int -> subtitle frame -> unit) ->
  input container -> unit


(** Output *)

(** Open an output file. *)
val open_output : string -> output container

(** Open an output format. *)
val open_output_format : (output, _) format -> output container

(** Open an output format by name. *)
val open_output_format_name : string -> output container

(** Close an output. *)
val close_output : output container -> unit

(** Set output tag list. *)
val set_output_metadata : output container -> (string * string) list -> unit
(** This must be set before starting writing streams. *)

(** Set output stream tag list. *)
val set_metadata : (output, _)stream -> (string * string) list -> unit
(** This must be set before starting writing streams. *)

(** Add a new audio stream to a media file. *)
val new_audio_stream : ?codec_id:Avcodec.Audio.id -> ?codec_name:string -> ?channel_layout:Channel_layout.t -> ?sample_format:Sample_format.t -> ?bit_rate:int -> ?sample_rate:int -> ?codec:audio Avcodec.t -> output container -> (output, audio)stream
(** Parameters passed unitarily take precedence over those of the codec. This must be set before starting writing streams. *)

(** Add a new video stream to a media file. *)
val new_video_stream : ?codec_id:Avcodec.Video.id -> ?codec_name:string -> ?width:int -> ?height:int -> ?pixel_format:Pixel_format.t -> ?bit_rate:int -> ?frame_rate:int -> ?codec:video Avcodec.t -> output container -> (output, video)stream
(** Parameters passed unitarily take precedence over those of the codec. This must be set before starting writing streams. *)

(** Add a new subtitle stream to a media file. *)
val new_subtitle_stream : ?codec_id:Avcodec.Subtitle.id -> ?codec_name:string -> ?codec:subtitle Avcodec.t -> output container -> (output, subtitle)stream
(** Parameters passed unitarily take precedence over those of the codec. This must be set before starting writing streams. *)

(** Write a frame to the output stream *)
val write : (output, 'media)stream -> 'media frame -> unit
