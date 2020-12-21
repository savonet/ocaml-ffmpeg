open Avutil

external init : unit -> unit = "ocaml_av_init" [@@noalloc]

let () = init ()

(* Format *)
module Format = struct
  external get_input_name : (input, _) format -> string
    = "ocaml_av_input_format_get_name"

  external get_input_long_name : (input, _) format -> string
    = "ocaml_av_input_format_get_long_name"

  external find_input_format : string -> (input, 'a) format
    = "ocaml_av_find_input_format"

  let find_input_format name =
    try Some (find_input_format name) with Not_found -> None

  external get_output_name : (output, _) format -> string
    = "ocaml_av_output_format_get_name"

  external get_output_long_name : (output, _) format -> string
    = "ocaml_av_output_format_get_long_name"

  external get_audio_codec_id : (output, audio) format -> Avcodec.Audio.id
    = "ocaml_av_output_format_get_audio_codec_id"

  external get_video_codec_id : (output, video) format -> Avcodec.Video.id
    = "ocaml_av_output_format_get_video_codec_id"

  external get_subtitle_codec_id :
    (output, subtitle) format -> Avcodec.Subtitle.id
    = "ocaml_av_output_format_get_subtitle_codec_id"

  external guess_output_format :
    string -> string -> string -> (output, 'a) format option
    = "ocaml_av_output_format_guess"

  let guess_output_format ?(short_name = "") ?(filename = "") ?(mime = "") () =
    guess_output_format short_name filename mime
end

(* Input *)
external open_input :
  string ->
  (input, _) format option ->
  (string * string) array ->
  input container * string array = "ocaml_av_open_input"

let open_input ?format ?opts url =
  let opts = opts_default opts in
  let ret, unused = open_input url format (mk_opts_array opts) in
  filter_opts unused opts;
  ret

type avio
type read = bytes -> int -> int -> int
type write = bytes -> int -> int -> int
type _seek = int -> int -> int
type seek = int -> Unix.seek_command -> int

let seek_of_int = function
  | 0 -> Unix.SEEK_SET
  | 1 -> Unix.SEEK_CUR
  | 2 -> Unix.SEEK_END
  | _ -> assert false

external ocaml_av_create_io :
  int -> read option -> write option -> _seek option -> avio
  = "ocaml_av_create_io"

let _seek_of_seek = function
  | None -> None
  | Some fn -> Some (fun a m -> fn a (seek_of_int m))

let ocaml_av_create_read_io len ?seek read =
  ocaml_av_create_io len (Some read) None (_seek_of_seek seek)

external ocaml_av_open_input_stream :
  avio ->
  (input, _) format option ->
  (string * string) array ->
  input container * string array = "ocaml_av_open_input_stream"

let ocaml_av_open_input_stream ?format ?opts avio =
  let opts = opts_default opts in
  let ret, unused =
    ocaml_av_open_input_stream avio format (mk_opts_array opts)
  in
  filter_opts unused opts;
  ret

external caml_av_input_io_finalise : avio -> unit = "caml_av_input_io_finalise"

let open_input_stream ?format ?opts ?seek read =
  let avio = ocaml_av_create_read_io 4096 ?seek read in
  let cleanup () = caml_av_input_io_finalise avio in
  let input = ocaml_av_open_input_stream ?format ?opts avio in
  Gc.finalise_last cleanup input;
  input

external _get_duration : input container -> int -> Time_format.t -> Int64.t
  = "ocaml_av_get_duration"

let get_input_duration ?(format = `Second) i =
  match _get_duration i (-1) format with 0L -> None | d -> Some d

external _get_metadata : input container -> int -> (string * string) list
  = "ocaml_av_get_metadata"

let get_input_metadata i = List.rev (_get_metadata i (-1))

(* Input Stream *)
type ('a, 'b, 'c) stream = { container : 'a container; index : int }
type media_type = MT_audio | MT_video | MT_subtitle

let mk_stream container index = { container; index }

external get_codec_params : (_, 'm, _) stream -> 'm Avcodec.params
  = "ocaml_av_get_stream_codec_parameters"

external get_time_base : (_, _, _) stream -> Avutil.rational
  = "ocaml_av_get_stream_time_base"

external set_time_base : (_, _, _) stream -> Avutil.rational -> unit
  = "ocaml_av_set_stream_time_base"

external get_frame_size : (_, audio, _) stream -> int
  = "ocaml_av_get_stream_frame_size"

external get_pixel_aspect : (_, video, _) stream -> Avutil.rational
  = "ocaml_av_get_stream_pixel_aspect"

external _get_streams : input container -> media_type -> int list
  = "ocaml_av_get_streams"

let get_streams input media_type =
  _get_streams input media_type
  |> List.rev_map (fun i ->
         let s = mk_stream input i in
         (i, s, get_codec_params s))

let get_audio_streams input = get_streams input MT_audio
let get_video_streams input = get_streams input MT_video
let get_subtitle_streams input = get_streams input MT_subtitle

external _find_best_stream : input container -> media_type -> int
  = "ocaml_av_find_best_stream"

let find_best_stream c t =
  let i = _find_best_stream c t in
  let s = mk_stream c i in
  (i, s, get_codec_params s)

let find_best_audio_stream c = find_best_stream c MT_audio
let find_best_video_stream c = find_best_stream c MT_video
let find_best_subtitle_stream c = find_best_stream c MT_subtitle
let get_input s = s.container
let get_index s = s.index

let get_duration ?(format = `Second) s =
  _get_duration s.container s.index format

let get_metadata s = List.rev (_get_metadata s.container s.index)

type input_result =
  [ `Audio_packet of int * audio Avcodec.Packet.t
  | `Audio_frame of int * audio frame
  | `Video_packet of int * video Avcodec.Packet.t
  | `Video_frame of int * video frame
  | `Subtitle_packet of int * subtitle Avcodec.Packet.t
  | `Subtitle_frame of int * subtitle frame ]

(** Reads the selected streams if any or all streams otherwise. *)
external read_input : int array -> int array -> input container -> input_result
  = "ocaml_av_read_input"

let get_id input =
  List.map (fun { index; container } ->
      if container != input then
        raise (Failure "Inconsistent stream and input!");
      index)

let read_input ?(audio_packet = []) ?(audio_frame = []) ?(video_packet = [])
    ?(video_frame = []) ?(subtitle_packet = []) ?(subtitle_frame = []) input =
  let packet =
    Array.of_list
      ( get_id input audio_packet @ get_id input video_packet
      @ get_id input subtitle_packet )
  in
  let frame =
    Array.of_list
      ( get_id input audio_frame @ get_id input video_frame
      @ get_id input subtitle_frame )
  in
  read_input packet frame input

type seek_flag =
  | Seek_flag_backward
  | Seek_flag_byte
  | Seek_flag_any
  | Seek_flag_frame

external seek :
  (input, _, _) stream -> Time_format.t -> Int64.t -> seek_flag array -> unit
  = "ocaml_av_seek_frame"

(* Output *)
external open_output :
  ?format:(output, _) format ->
  string ->
  (string * string) array ->
  output container * string array = "ocaml_av_open_output"

let open_output ?format ?opts fname =
  let opts = opts_default opts in
  let ret, unused = open_output ?format fname (mk_opts_array opts) in
  filter_opts unused opts;
  ret

external ocaml_av_open_output_stream :
  (output, _) format ->
  avio ->
  (string * string) array ->
  output container * string array = "ocaml_av_open_output_stream"

let open_output_stream ?opts ?seek write format =
  let opts = opts_default opts in
  let avio = ocaml_av_create_io 4096 None (Some write) (_seek_of_seek seek) in
  let cleanup () = caml_av_input_io_finalise avio in
  let output, unused =
    ocaml_av_open_output_stream format avio (mk_opts_array opts)
  in
  filter_opts unused opts;
  Gc.finalise_last cleanup output;
  output

external output_started : output container -> bool = "ocaml_av_header_written"

external _set_metadata : _ container -> int -> (string * string) array -> unit
  = "ocaml_av_set_metadata"

let set_output_metadata o tags = _set_metadata o (-1) (Array.of_list tags)
let set_metadata s tags = _set_metadata s.container s.index (Array.of_list tags)
let get_output s = s.container

external new_stream_copy : output container -> _ Avcodec.params -> int
  = "ocaml_av_new_stream_copy"

let new_stream_copy ~params container =
  let ret = new_stream_copy container params in
  mk_stream container ret

external new_audio_stream :
  _ container ->
  int ->
  [ `Encoder ] Avcodec.Audio.t ->
  (string * string) array ->
  int * string array = "ocaml_av_new_audio_stream"

let new_audio_stream ?opts ?channels ?channel_layout ~sample_rate ~sample_format
    ~time_base ~codec container =
  let opts =
    mk_audio_opts ?opts ?channels ?channel_layout ~sample_rate ~sample_format
      ~time_base
  in
  let ret, unused =
    new_audio_stream container
      (Sample_format.get_id sample_format)
      codec (mk_opts_array opts)
  in
  filter_opts unused opts;
  mk_stream container ret

external new_video_stream :
  ?device_context:Avutil.HwContext.device_context ->
  ?frame_context:Avutil.HwContext.frame_context ->
  _ container ->
  [ `Encoder ] Avcodec.Video.t ->
  (string * string) array ->
  int * string array = "ocaml_av_new_video_stream"

let new_video_stream ?opts ?frame_rate ?hardware_context ~pixel_format ~width
    ~height ~time_base ~codec container =
  let opts =
    mk_video_opts ?opts ?frame_rate ~pixel_format ~width ~height ~time_base
  in
  let device_context, frame_context =
    match hardware_context with
      | None -> (None, None)
      | Some (`Device_context hardware_context) -> (Some hardware_context, None)
      | Some (`Frame_context frame_context) -> (None, Some frame_context)
  in
  let ret, unused =
    new_video_stream ?device_context ?frame_context container codec
      (mk_opts_array opts)
  in
  filter_opts unused opts;
  mk_stream container ret

external new_subtitle_stream :
  _ container ->
  [ `Encoder ] Avcodec.Subtitle.t ->
  (string * string) array ->
  int * string array = "ocaml_av_new_subtitle_stream"

let new_subtitle_stream ?opts ~time_base ~codec container =
  let opts = opts_default opts in
  Hashtbl.add opts "time_base" (`String (Avutil.string_of_rational time_base));
  let ret, unused = new_subtitle_stream container codec (mk_opts_array opts) in
  filter_opts unused opts;
  mk_stream container ret

external codec_attr : _ stream -> string option = "ocaml_av_codec_attr"
external bitrate : _ stream -> int option = "ocaml_av_stream_bitrate"

external write_packet :
  (output, 'media, [ `Packet ]) stream ->
  Avutil.rational ->
  'media Avcodec.Packet.t ->
  unit = "ocaml_av_write_stream_packet"

external write_frame :
  (output, 'media, [ `Frame ]) stream -> 'media frame -> unit
  = "ocaml_av_write_stream_frame"

external flush : output container -> unit = "ocaml_av_flush"
external was_keyframe : (output, _, _) stream -> bool = "ocaml_av_was_keyframe"

external write_audio_frame : output container -> audio frame -> unit
  = "ocaml_av_write_audio_frame"

external write_video_frame : output container -> video frame -> unit
  = "ocaml_av_write_video_frame"

external close : _ container -> unit = "ocaml_av_close"
