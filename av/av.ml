open Avutil

external init : unit -> unit = "ocaml_av_init" [@@noalloc]

let () = init ()

external container_options : unit -> Options.t = "ocaml_av_container_options"

let container_options = container_options ()

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

external ocaml_av_cleanup_av : _ container -> unit = "ocaml_av_cleanup_av"

(* Input *)
external open_input :
  string ->
  (input, _) format option ->
  (unit -> bool) option ->
  (string * string) array ->
  input container * string array = "ocaml_av_open_input"

let open_input ?interrupt ?format ?opts url =
  let opts = opts_default opts in
  let ret, unused = open_input url format interrupt (mk_opts_array opts) in
  filter_opts unused opts;
  Gc.finalise ocaml_av_cleanup_av ret;
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
  Gc.finalise ocaml_av_cleanup_av ret;
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

external input_obj : input container -> 'a = "ocaml_av_input_obj"

let input_obj c = Obj.magic (input_obj c, c)

(* Input Stream *)
type ('a, 'b, 'c) stream = {
  container : 'a container;
  index : int;
  mutable decoder : ('b, Avcodec.decode) Avcodec.codec option;
}

type media_type = MT_audio | MT_video | MT_data | MT_subtitle

let mk_stream container index = { container; index; decoder = None }

external get_codec_params : (_, 'm, _) stream -> 'm Avcodec.params
  = "ocaml_av_get_stream_codec_parameters"

external get_time_base : (_, _, _) stream -> Avutil.rational
  = "ocaml_av_get_stream_time_base"

external set_time_base : (_, _, _) stream -> Avutil.rational -> unit
  = "ocaml_av_set_stream_time_base"

external get_frame_size : (_, audio, _) stream -> int
  = "ocaml_av_get_stream_frame_size"

external get_pixel_aspect : (_, video, _) stream -> Avutil.rational option
  = "ocaml_av_get_stream_pixel_aspect"

external _get_streams : _ container -> media_type -> int list
  = "ocaml_av_get_streams"

let get_streams container media_type =
  _get_streams container media_type
  |> List.rev_map (fun i ->
         let s = mk_stream container i in
         (i, s, get_codec_params s))

let get_audio_streams container = get_streams container MT_audio
let get_video_streams container = get_streams container MT_video
let get_subtitle_streams container = get_streams container MT_subtitle
let get_data_streams container = get_streams container MT_data
let set_decoder s decoder = s.decoder <- Some decoder

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
  | `Subtitle_frame of int * subtitle frame
  | `Data_packet of int * [ `Data ] Avcodec.Packet.t ]

(** Reads the selected streams if any or all streams otherwise. *)
external read_input :
  (int * Avutil.media_type) array ->
  (int * ('a, Avcodec.decode) Avcodec.codec option) array ->
  input container ->
  input_result = "ocaml_av_read_input"

let _get_packet media_type input =
  List.map (fun { index; container; _ } ->
      if container != input then
        raise (Failure "Inconsistent stream and input!");
      (index, media_type))

let _get_frame input =
  List.map (fun { index; container; decoder } ->
      if container != input then
        raise (Failure "Inconsistent stream and input!");
      (index, Obj.magic decoder))

let read_input ?(audio_packet = []) ?(audio_frame = []) ?(video_packet = [])
    ?(video_frame = []) ?(subtitle_packet = []) ?(subtitle_frame = [])
    ?(data_packet = []) input =
  let packet =
    Array.of_list
      (_get_packet `Audio input audio_packet
      @ _get_packet `Video input video_packet
      @ _get_packet `Subtitle input subtitle_packet
      @ _get_packet `Data input data_packet)
  in
  let frame =
    Array.of_list
      (_get_frame input audio_frame
      @ _get_frame input video_frame
      @ _get_frame input subtitle_frame)
  in
  read_input packet frame input

type seek_flag =
  | Seek_flag_backward
  | Seek_flag_byte
  | Seek_flag_any
  | Seek_flag_frame

external seek :
  flags:seek_flag array ->
  ?stream:(input, _, _) stream ->
  ?min_ts:Int64.t ->
  ?max_ts:Int64.t ->
  fmt:Time_format.t ->
  ts:Int64.t ->
  input container ->
  unit = "ocaml_av_seek_bytecode" "ocaml_av_seek_native"

let seek ?(flags = []) = seek ~flags:(Array.of_list flags)

(* Output *)
external open_output :
  ?interrupt:(unit -> bool) ->
  ?format:(output, _) format ->
  string ->
  bool ->
  (string * string) array ->
  output container * string array = "ocaml_av_open_output"

let open_output ?interrupt ?format ?(interleaved = true) ?opts fname =
  let opts = opts_default opts in
  let ret, unused =
    open_output ?interrupt ?format fname interleaved (mk_opts_array opts)
  in
  filter_opts unused opts;
  Gc.finalise ocaml_av_cleanup_av ret;
  ret

external ocaml_av_open_output_stream :
  (output, _) format ->
  avio ->
  bool ->
  (string * string) array ->
  output container * string array = "ocaml_av_open_output_stream"

let open_output_stream ?opts ?(interleaved = true) ?seek write format =
  let opts = opts_default opts in
  let avio = ocaml_av_create_io 4096 None (Some write) (_seek_of_seek seek) in
  let cleanup () = caml_av_input_io_finalise avio in
  let output, unused =
    ocaml_av_open_output_stream format avio interleaved (mk_opts_array opts)
  in
  filter_opts unused opts;
  Gc.finalise ocaml_av_cleanup_av output;
  Gc.finalise_last cleanup output;
  output

external output_started : output container -> bool = "ocaml_av_header_written"

external _set_metadata : _ container -> int -> (string * string) array -> unit
  = "ocaml_av_set_metadata"

let set_output_metadata o tags = _set_metadata o (-1) (Array.of_list tags)
let set_metadata s tags = _set_metadata s.container s.index (Array.of_list tags)
let get_output s = s.container

type uninitialized_stream_copy = output container * int

external new_uninitialized_stream_copy : output container -> int
  = "ocaml_av_new_uninitialized_stream_copy"

let new_uninitialized_stream_copy container =
  (container, new_uninitialized_stream_copy container)

external initialize_stream_copy :
  output container -> int -> _ Avcodec.params -> unit
  = "ocaml_av_initialize_stream_copy"

let initialize_stream_copy ~params (container, index) =
  initialize_stream_copy container index params;
  mk_stream container index

let new_stream_copy ~params container =
  initialize_stream_copy ~params (new_uninitialized_stream_copy container)

external new_audio_stream :
  _ container ->
  int ->
  [ `Encoder ] Avcodec.Audio.t ->
  int ->
  (string * string) array ->
  int * string array = "ocaml_av_new_audio_stream"

let new_audio_stream ?opts ?channels ?channel_layout ~sample_rate ~sample_format
    ~time_base ~codec container =
  let opts =
    mk_audio_opts ?opts ?channels ?channel_layout ~sample_rate ~sample_format
      ~time_base ()
  in
  let channels =
    match (channels, channel_layout) with
      | Some n, _ -> n
      | None, Some layout -> Avutil.Channel_layout.get_nb_channels layout
      | None, None ->
          raise
            (Error
               (`Failure
                 "At least one of channels or channel_layout must be passed!"))
  in
  let ret, unused =
    new_audio_stream container
      (Sample_format.get_id sample_format)
      codec channels (mk_opts_array opts)
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
    mk_video_opts ?opts ?frame_rate ~pixel_format ~width ~height ~time_base ()
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

external new_data_stream :
  _ container -> Avcodec.Unknown.id -> Avutil.rational -> int
  = "ocaml_av_new_data_stream"

let new_data_stream ~time_base ~codec container =
  let ret = new_data_stream container codec time_base in
  mk_stream container ret

external codec_attr : _ stream -> string option = "ocaml_av_codec_attr"
external bitrate : _ stream -> int option = "ocaml_av_stream_bitrate"

external write_packet :
  (output, 'media, [ `Packet ]) stream ->
  Avutil.rational ->
  'media Avcodec.Packet.t ->
  unit = "ocaml_av_write_stream_packet"

external write_frame :
  ?on_keyframe:(unit -> unit) ->
  (output, 'media, [ `Frame ]) stream ->
  'media frame ->
  unit = "ocaml_av_write_stream_frame"

external flush : output container -> unit = "ocaml_av_flush"
external close : _ container -> unit = "ocaml_av_close"
