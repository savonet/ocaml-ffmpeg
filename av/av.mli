(** This module perform demuxing then decoding for reading and coding then
    muxing for writing multimedia container formats. *)

open Avutil

(** {5 Format} *)

module Format : sig
  (** Return the name of the input format *)
  val get_input_name : (input, _) format -> string

  (** Return the long name of the input format *)
  val get_input_long_name : (input, _) format -> string

  (** Return the name of the output format *)
  val get_output_name : (output, _) format -> string

  (** Return the long name of the output format *)
  val get_output_long_name : (output, _) format -> string

  (** Guess output format based on the passed arguments. *)
  val guess_output_format :
    ?short_name:string ->
    ?filename:string ->
    ?mime:string ->
    unit ->
    (output, 'a) format option

  (** Return the audio codec id of the output audio format *)
  val get_audio_codec_id : (output, audio) format -> Avcodec.Audio.id

  (** Return the video codec id of the output video format *)
  val get_video_codec_id : (output, video) format -> Avcodec.Video.id

  (** Return the subtitle codec id of the output subtitle format *)
  val get_subtitle_codec_id : (output, subtitle) format -> Avcodec.Subtitle.id
end

(** {5 Input} *)

(** [Av.open_input url] open the input [url] (a file name or http URL). Raise
    Error if the opening failed. *)
val open_input : string -> input container

(** [Av.open_input_format format] open the input [format]. Raise Error if the
    opening failed. *)
val open_input_format : (input, _) format -> input container

type read = bytes -> int -> int -> int
type write = bytes -> int -> int -> int
type seek = int -> Unix.seek_command -> int

val open_input_stream : ?seek:seek -> read -> input container

(** [Av.get_input_duration ~format:fmt input] return the duration of an [input]
    in the [fmt] time format (in second by default). *)
val get_input_duration : ?format:Time_format.t -> input container -> Int64.t

(** Return the input tag (key, vlue) list. *)
val get_input_metadata : input container -> (string * string) list

(** Input/output, audio/video/subtitle stream type *)
type ('line, 'media) stream

(** Return the audio stream list of the input. The result is a list of tuple
    containing the index of the stream in the container, the stream and the
    codec of the stream. *)
val get_audio_streams :
  input container -> (int * (input, audio) stream * audio Avcodec.params) list

(** Same as {!Av.get_audio_streams} for the video streams. *)
val get_video_streams :
  input container -> (int * (input, video) stream * video Avcodec.params) list

(** Same as {!Av.get_audio_streams} for the subtitle streams. *)
val get_subtitle_streams :
  input container ->
  (int * (input, subtitle) stream * subtitle Avcodec.params) list

(** Return the best audio stream of the input. The result is a tuple containing
    the index of the stream in the container, the stream and the codec of the
    stream. Raise Error if no stream could be found. *)
val find_best_audio_stream :
  input container -> int * (input, audio) stream * audio Avcodec.params

(** Same as {!Av.find_best_audio_stream} for the video streams. *)
val find_best_video_stream :
  input container -> int * (input, video) stream * video Avcodec.params

(** Same as {!Av.find_best_audio_stream} for the subtitle streams. *)
val find_best_subtitle_stream :
  input container -> int * (input, subtitle) stream * subtitle Avcodec.params

(** Return the input container of the input stream. *)
val get_input : (input, _) stream -> input container

(** Return the index of the stream. *)
val get_index : (_, _) stream -> int

(** [Av.get_codec stream] return the codec of the [stream]. Raise Error if the
    codec allocation failed. *)
val get_codec_params : (_, 'media) stream -> 'media Avcodec.params

(** [Av.get_time_base stream] return the time base of the [stream]. *)
val get_time_base : (_, _) stream -> Avutil.rational

(** [Av.set_time_base stream time_base] set the [stream] time base to
    [time_base]. *)
val set_time_base : (_, _) stream -> Avutil.rational -> unit

(** [Av.get_pixel_aspect stream] return the pixel aspect of the [stream]. *)
val get_pixel_aspect : (_, video) stream -> Avutil.rational

(** Same as {!Av.get_input_duration} for the input streams. *)
val get_duration : ?format:Time_format.t -> (input, _) stream -> Int64.t

(** Same as {!Av.get_input_metadata} for the input streams. *)
val get_metadata : (input, _) stream -> (string * string) list

(** [Av.select stream] select the input [stream] for reading. Raise Error if the
    selection failed. *)
val select : (input, _) stream -> unit

(** [Av.read_packet stream] read the input [stream]. Return the next packet of
    the [stream] or raises [Error `Eof] if the end of the stream is reached.
    Raise Error if the reading failed. *)
val read_packet : (input, 'media) stream -> 'media Avcodec.Packet.t

(** [Av.iter_packet f is] applies function [f] in turn to all the packets of the
    input stream [is]. Raise Error if the reading failed. *)
val iter_packet :
  ('media Avcodec.Packet.t -> unit) -> (input, 'media) stream -> unit

(** [Av.read_frame stream] read the input [stream]. Return the next frame of the
    [stream] or raises [Error `Eof] if the end of the stream is reached. Raise
    Error if the reading failed. *)
val read_frame : (input, 'media) stream -> 'media frame

(** [Av.iter_frame f is] applies function [f] in turn to all the frames of the
    input stream [is]. Raise Error if the reading failed. *)
val iter_frame : ('media frame -> unit) -> (input, 'media) stream -> unit

type input_packet_result =
  [ `Audio of int * audio Avcodec.Packet.t
  | `Video of int * video Avcodec.Packet.t
  | `Subtitle of int * subtitle Avcodec.Packet.t ]

(** Reads the selected streams if any or all streams otherwise. Return the next
    [Audio] [Video] or [Subtitle] index and packet of the input or [End_of_file]
    if the end of the input is reached. Raise Error if the reading failed. *)
val read_input_packet : input container -> input_packet_result

(** [Av.iter_input_packet ~audio:af ~video:vf ~subtitle:sf src] reads
    iteratively the selected streams if any or all streams of the [src] input
    otherwise. It applies function [af] to the audio packets, [vf] to the video
    packets and [sf] to the subtitle packets with the index of the related
    stream as first parameter. Raise Error if the reading failed. *)
val iter_input_packet :
  ?audio:(int -> audio Avcodec.Packet.t -> unit) ->
  ?video:(int -> video Avcodec.Packet.t -> unit) ->
  ?subtitle:(int -> subtitle Avcodec.Packet.t -> unit) ->
  input container ->
  unit

type input_frame_result =
  [ `Audio of int * audio frame
  | `Video of int * video frame
  | `Subtitle of int * subtitle frame ]

(** Reads the selected streams if any or all streams otherwise. Return the next
    [Audio] [Video] or [Subtitle] index and frame of the input or raises
    [Error `Eof] if the end of the input is reached. Raise Error if the reading
    failed. *)
val read_input_frame : input container -> input_frame_result

(** [Av.iter_input_frame ~audio:af ~video:vf ~subtitle:sf src] reads iteratively
    the selected streams if any or all streams of the [src] input otherwise. It
    applies function [af] to the audio frames, [vf] to the video frames and [sf]
    to the subtitle frames with the index of the related stream as first
    parameter. Raise Error if the reading failed. *)
val iter_input_frame :
  ?audio:(int -> audio frame -> unit) ->
  ?video:(int -> video frame -> unit) ->
  ?subtitle:(int -> subtitle frame -> unit) ->
  input container ->
  unit

(** Seek mode. *)
type seek_flag =
  | Seek_flag_backward
  | Seek_flag_byte
  | Seek_flag_any
  | Seek_flag_frame

(** [Av.seek is fmt t flags] seek in the input stream [is] at the position [t]
    in the [fmt] time format according to the method indicated by the [flags].
    Raise Error if the seeking failed. *)
val seek :
  (input, _) stream -> Time_format.t -> Int64.t -> seek_flag array -> unit

(** [Av.reuse_output ro] enables or disables the reuse of {!Av.read_packet},
    {!Av.iter_packet}, {!Av.read_frame}, {!Av.iter_frame},
    {!Av.read_input_packet}, {!Av.iter_input_packet}, {!Av.read_input_frame} and
    {!Av.iter_input_frame} output according to the value of [ro]. Reusing the
    output reduces the number of memory allocations. In this cas, the data
    returned by a reading function is invalidated by a new call to this
    function. *)
val reuse_output : input container -> bool -> unit

(** {5 Output} *)

(** [Av.open_output ?format ?opts filename] open the output file named
    [filename]. [format] may contain an optional format, [opts] may contain any
    option settable on the stream's internal AVFormat. After returning, if
    [opts] was passed, unused options are left in the hash table. Raise Error if
    the opening failed. *)
val open_output :
  ?format:(output, _) format -> ?opts:opts -> string -> output container

(** [Av.open_stream callbacks] open the output container with the given
    callbacks. [opts] may contain any option settable on Ffmpeg avformat. After
    returning, if [opts] was passed, unused options are left in the hash table.
    Raise Error if the opening failed. *)
val open_output_stream :
  ?opts:opts -> ?seek:seek -> write -> (output, _) format -> output container

(** Returns [true] if the output has already started, in which case no new *
    stream or metadata can be added. *)
val output_started : output container -> bool

(** [Av.set_output_metadata dst tags] set the metadata of the [dst] output with
    the [tags] tag list. This must be set before starting writing streams. Raise
    Error if a writing already taken place or if the setting failed. *)
val set_output_metadata : output container -> (string * string) list -> unit

(** Same as {!Av.set_output_metadata} for the output streams. *)
val set_metadata : (output, _) stream -> (string * string) list -> unit

(** Return the output container of the output stream. *)
val get_output : (output, _) stream -> output container

(** Utility to convert audio params to options. Sets value first from named
    arguments or from passed codec_params, if it exists, or from passed stream,
    if it exists. *)
val mk_audio_opts :
  ?channels:int ->
  ?channel_layout:Channel_layout.t ->
  ?bit_rate:int ->
  ?sample_rate:int ->
  ?sample_format:Avutil.Sample_format.t ->
  ?time_base:Avutil.rational ->
  ?params:audio Avcodec.params ->
  ?stream:(_, audio) stream ->
  unit ->
  opts

(** Add a new audio stream to the given container. [opts] may contain any option
    settable on the stream's internal AVCodec. After returning, if [opts] was
    passed, unused options are left in the hash table.

    Raise Error if the opening failed. *)
val new_audio_stream :
  ?opts:opts ->
  codec:[ `Encoder ] Avcodec.Audio.t ->
  output container ->
  (output, audio) stream

(** Utility to convert video params to options. *)
val mk_video_opts :
  ?pixel_format:Avutil.Pixel_format.t ->
  ?size:int * int ->
  ?bit_rate:int ->
  ?frame_rate:int ->
  ?time_base:Avutil.rational ->
  ?params:video Avcodec.params ->
  ?stream:(_, video) stream ->
  unit ->
  opts

(** Add a new video stream to the given container. [opts] may contain any option
    settable on the stream's internal AVCodec. After returning, if [opts] was
    passed, unused options are left in the hash table.

    Raise Error if the opening failed. *)
val new_video_stream :
  ?opts:opts ->
  codec:[ `Encoder ] Avcodec.Video.t ->
  output container ->
  (output, video) stream

(** Utility to convert subtitle params to options. *)
val mk_subtitle_opts : ?time_base:Avutil.rational -> unit -> opts

(** Add a new subtitle stream to the given container. [opts] may contain any
    option settable on the stream's internal AVCodec. After returning, if [opts]
    was passed, unused options are left in the hash table.

    Raise Error if the opening failed. *)
val new_subtitle_stream :
  ?opts:opts ->
  codec:[ `Encoder ] Avcodec.Subtitle.t ->
  output container ->
  (output, subtitle) stream

(** [Av.write_packet os pkt] write the [pkt] packet to the [os] output stream.
    Raise Error if the writing failed. *)
val write_packet : (output, 'media) stream -> 'media Avcodec.Packet.t -> unit

(** [Av.write_frame os frm] write the [frm] frame to the [os] output stream.
    Raise Error if the writing failed. *)
val write_frame : (output, 'media) stream -> 'media frame -> unit

(** [Av.write_audio_frame dst frm] write the [frm] audio frame to the [dst]
    output audio container. Raise Error if the output format is not defined or
    if the output media type is not compatible with the frame or if the writing
    failed. *)
val write_audio_frame : output container -> audio frame -> unit

(** Same as {!Av.write_audio_frame} for output video container. *)
val write_video_frame : output container -> video frame -> unit

(** Close an input or output container. *)
val close : _ container -> unit
