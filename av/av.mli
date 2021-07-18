(** This module perform demuxing then decoding for reading and coding then
    muxing for writing multimedia container formats. *)

open Avutil

(* A value suitable for listing available options on a given container. *)
val container_options : Options.t

(** {5 Format} *)

module Format : sig
  (** Return the name of the input format *)
  val get_input_name : (input, _) format -> string

  (** Return the long name of the input format *)
  val get_input_long_name : (input, _) format -> string

  (** Guess input format based on its short name. *)
  val find_input_format : string -> (input, 'a) format option

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

(** [Av.open_input url] open the input [url] (a file name or http URL). After
    returning, if [opts] was passed, unused options are left in the hash table.
    Raise Error if the opening failed. *)
val open_input :
  ?format:(input, _) format -> ?opts:opts -> string -> input container

type read = bytes -> int -> int -> int
type write = bytes -> int -> int -> int
type seek = int -> Unix.seek_command -> int

val open_input_stream :
  ?format:(input, _) format ->
  ?opts:opts ->
  ?seek:seek ->
  read ->
  input container

(** [Av.get_input_duration ~format:fmt input] return the duration of an [input]
    in the [fmt] time format (in second by default). *)
val get_input_duration :
  ?format:Time_format.t -> input container -> Int64.t option

(** Return the input tag (key, vlue) list. *)
val get_input_metadata : input container -> (string * string) list

(** Return a value of type [obj], suited for use with [Avutils.Options] getters. *)
val input_obj : input container -> Options.obj

(** Input/output, audio/video/subtitle, mode stream type *)
type ('line, 'media, 'mode) stream

(** Return the audio stream list of the input. The result is a list of tuple
    containing the index of the stream in the container, the stream and the
    codec of the stream. *)
val get_audio_streams :
  input container ->
  (int * (input, audio, 'a) stream * audio Avcodec.params) list

(** Same as {!Av.get_audio_streams} for the video streams. *)
val get_video_streams :
  input container ->
  (int * (input, video, 'a) stream * video Avcodec.params) list

(** Same as {!Av.get_audio_streams} for the subtitle streams. *)
val get_subtitle_streams :
  input container ->
  (int * (input, subtitle, 'a) stream * subtitle Avcodec.params) list

(** Return the best audio stream of the input. The result is a tuple containing
    the index of the stream in the container, the stream and the codec of the
    stream. Raise Error if no stream could be found. *)
val find_best_audio_stream :
  input container -> int * (input, audio, 'a) stream * audio Avcodec.params

(** Same as {!Av.find_best_audio_stream} for the video streams. *)
val find_best_video_stream :
  input container -> int * (input, video, 'a) stream * video Avcodec.params

(** Same as {!Av.find_best_audio_stream} for the subtitle streams. *)
val find_best_subtitle_stream :
  input container ->
  int * (input, subtitle, 'a) stream * subtitle Avcodec.params

(** Return the input container of the input stream. *)
val get_input : (input, _, _) stream -> input container

(** Return the index of the stream. *)
val get_index : (_, _, _) stream -> int

(** [Av.get_codec stream] return the codec of the [stream]. Raise Error if the
    codec allocation failed. *)
val get_codec_params : (_, 'media, _) stream -> 'media Avcodec.params

(** [Av.get_time_base stream] return the time base of the [stream]. *)
val get_time_base : (_, _, _) stream -> Avutil.rational

(** [Av.set_time_base stream time_base] set the [stream] time base to
    [time_base]. *)
val set_time_base : (_, _, _) stream -> Avutil.rational -> unit

(** [Av.get_frame_size stream] return the frame size for the given audio stream. *)
val get_frame_size : (output, audio, _) stream -> int

(** [Av.get_pixel_aspect stream] return the pixel aspect of the [stream]. *)
val get_pixel_aspect : (_, video, _) stream -> Avutil.rational

(** Same as {!Av.get_input_duration} for the input streams. *)
val get_duration : ?format:Time_format.t -> (input, _, _) stream -> Int64.t

(** Same as {!Av.get_input_metadata} for the input streams. *)
val get_metadata : (input, _, _) stream -> (string * string) list

type input_result =
  [ `Audio_packet of int * audio Avcodec.Packet.t
  | `Audio_frame of int * audio frame
  | `Video_packet of int * video Avcodec.Packet.t
  | `Video_frame of int * video frame
  | `Subtitle_packet of int * subtitle Avcodec.Packet.t
  | `Subtitle_frame of int * subtitle frame ]

(** Reads the selected streams if any or all streams otherwise. Return the next
    [Audio] [Video] or [Subtitle] index and packet or frame of the input or [Error `Eof]
    if the end of the input is reached. Raise Error if the reading failed.

    Only packet and frames from the specified streams are returned. *)
val read_input :
  ?audio_packet:(input, audio, [ `Packet ]) stream list ->
  ?audio_frame:(input, audio, [ `Frame ]) stream list ->
  ?video_packet:(input, video, [ `Packet ]) stream list ->
  ?video_frame:(input, video, [ `Frame ]) stream list ->
  ?subtitle_packet:(input, subtitle, [ `Packet ]) stream list ->
  ?subtitle_frame:(input, subtitle, [ `Frame ]) stream list ->
  input container ->
  input_result

(** Seek mode. *)
type seek_flag =
  | Seek_flag_backward
  | Seek_flag_byte
  | Seek_flag_any
  | Seek_flag_frame

(** [Av.seek ?flags ?stream ?min_ts ?max_ts ~fmt ~ts container] seek in the container [container]
    to position [ts]. You can pass an optional [stream] to use for seeking, [max_ts] and [min_ts]
    to force seeking to happen within a given timestamp window and [flags] to speficy certain
    property of the seeking operation. Raise Error if the seeking failed. *)
val seek :
  ?flags:seek_flag list ->
  ?stream:(input, _, _) stream ->
  ?min_ts:Int64.t ->
  ?max_ts:Int64.t ->
  fmt:Time_format.t ->
  ts:Int64.t ->
  input container ->
  unit

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
val set_metadata : (output, _, _) stream -> (string * string) list -> unit

(** Return the output container of the output stream. *)
val get_output : (output, _, _) stream -> output container

(* Create a new stream that only supports packet input and does not do any
   encoding. Used for remuxing with encoded data. *)
val new_stream_copy :
  params:'mode Avcodec.params ->
  output container ->
  (output, 'mode, [ `Packet ]) stream

(** Add a new audio stream to the given container. Stream only supports frames
    and encodes its input.

    [opts] may contain any option settable on the stream's internal AVCodec.
    After returning, if [opts] was passed, unused options are left in the hash
    table.

    At least one of [channels] or [channel_layout] must be passed.

    Frames passed to this stream for encoding must have a PTS set according to
    the given [time_base]. [1/sample_rate] is usually a good value for the
    [time_base].

    Please note that some codec require a fixed frame size, denoted by the
    absence of the [`Variable_frame_size] codec capabilities. In this case, the
    user is expected to pass frames containing exactly
    [Av.get_frame_size stream].

    [Avfilter] can be used to slice frames into frames of fixed size. See
    [Avfilter.Utils.convert_audio] for an example.

    Raise Error if the opening failed. *)
val new_audio_stream :
  ?opts:opts ->
  ?channels:int ->
  ?channel_layout:Channel_layout.t ->
  sample_rate:int ->
  sample_format:Avutil.Sample_format.t ->
  time_base:Avutil.rational ->
  codec:[ `Encoder ] Avcodec.Audio.t ->
  output container ->
  (output, audio, [ `Frame ]) stream

(** Add a new video stream to the given container. Stream only supports frames
    and encodes its input.

    [opts] may contain any option settable on the stream's internal AVCodec.
    After returning, if [opts] was passed, unused options are left in the hash
    table.

    Frames passed to this stream for encoding must have a PTS set according to
    the given [time_base]. [1/frame_rate] is usually a good value for the
    [time_base].

    [hardware_context] can be used to pass optional hardware device and frame
    context to enable hardward encoding on this stream.

    Raise Error if the opening failed. *)
val new_video_stream :
  ?opts:opts ->
  ?frame_rate:Avutil.rational ->
  ?hardware_context:Avcodec.Video.hardware_context ->
  pixel_format:Avutil.Pixel_format.t ->
  width:int ->
  height:int ->
  time_base:Avutil.rational ->
  codec:[ `Encoder ] Avcodec.Video.t ->
  output container ->
  (output, video, [ `Frame ]) stream

(** Add a new subtitle stream to the given container. Stream only supports frames
    and encodes its input.

    [opts] may contain any option settable on the stream's internal AVCodec.
    After returning, if [opts] was passed, unused options are left in the hash
    table.

    Raise Error if the opening failed. *)
val new_subtitle_stream :
  ?opts:opts ->
  time_base:Avutil.rational ->
  codec:[ `Encoder ] Avcodec.Subtitle.t ->
  output container ->
  (output, subtitle, [ `Frame ]) stream

(** Return a codec attribute suitable for HLS playlists when available. *)
val codec_attr : _ stream -> string option

(** Return the stream's bitrate when available, suitable for HLS playlists. *)
val bitrate : _ stream -> int option

(** [Av.write_packet os time_base pkt] write the [pkt] packet to the [os] output stream.
    [time_base] is the packet's PTS/DTS/duration time base.
    Raise Error if the writing failed. *)
val write_packet :
  (output, 'media, [ `Packet ]) stream ->
  Avutil.rational ->
  'media Avcodec.Packet.t ->
  unit

(** [Av.write_frame os frm] write the [frm] frame to the [os] output stream.

    Frame PTS should be set and counted in units of [time_base], as passed
    when creating the stream

    Raise Error if the writing failed. *)
val write_frame : (output, 'media, [ `Frame ]) stream -> 'media frame -> unit

(** [true] if the last processed frame was a video key frame. *)
val was_keyframe : (output, _, _) stream -> bool

(** [Av.write_audio_frame dst frm] write the [frm] audio frame to the [dst]
    output audio container. Raise Error if the output format is not defined or
    if the output media type is not compatible with the frame or if the writing
    failed. *)
val write_audio_frame : output container -> audio frame -> unit

(** Same as {!Av.write_audio_frame} for output video container. *)
val write_video_frame : output container -> video frame -> unit

(** Flush the underlying muxer. *)
val flush : output container -> unit

(** Close an input or output container. *)
val close : _ container -> unit
