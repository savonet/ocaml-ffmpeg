open Avutil

external init : unit -> unit = "ocaml_av_init" [@@noalloc]

let () = init ()

let _opt_val = function
  | `String s -> s
  | `Int i -> string_of_int i
  | `Float f -> string_of_float f

let opts_default = function None -> Hashtbl.create 0 | Some opts -> opts

let mk_opts opts =
  Array.of_list
    (Hashtbl.fold
       (fun opt_name opt_val cur -> (opt_name, _opt_val opt_val) :: cur)
       opts [])

let filter_opts unused opts =
  Hashtbl.filter_map_inplace
    (fun k v -> if Array.mem k unused then Some v else None)
    opts

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
  let ret, unused = open_input url format (mk_opts opts) in
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
  let ret, unused = ocaml_av_open_input_stream avio format (mk_opts opts) in
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

let get_input_duration ?(format = `Second) i = _get_duration i (-1) format

external _get_metadata : input container -> int -> (string * string) list
  = "ocaml_av_get_metadata"

let get_input_metadata i = List.rev (_get_metadata i (-1))

(* Input Stream *)
type ('a, 'b) stream = { container : 'a container; index : int }
type media_type = MT_audio | MT_video | MT_subtitle

let mk_stream container index = { container; index }

external get_codec_params : (_, 'm) stream -> 'm Avcodec.params
  = "ocaml_av_get_stream_codec_parameters"

external get_time_base : (_, _) stream -> Avutil.rational
  = "ocaml_av_get_stream_time_base"

external set_time_base : (_, _) stream -> Avutil.rational -> unit
  = "ocaml_av_set_stream_time_base"

external get_frame_size : (output, audio) stream -> int
  = "ocaml_av_get_stream_frame_size"

external get_pixel_aspect : (_, video) stream -> Avutil.rational
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

external select : (input, _) stream -> unit = "ocaml_av_select_stream"

external read_packet : (input, 'm) stream -> 'm Avcodec.Packet.t
  = "ocaml_av_read_stream_packet"

let rec iter_packet f stream =
  try
    f (read_packet stream);
    iter_packet f stream
  with Error `Eof -> ()

external read_frame : (input, 'm) stream -> 'm frame
  = "ocaml_av_read_stream_frame"

let rec iter_frame f s =
  try
    f (read_frame s);
    iter_frame f s
  with Error `Eof -> ()

type input_packet_result =
  [ `Audio of int * audio Avcodec.Packet.t
  | `Video of int * video Avcodec.Packet.t
  | `Subtitle of int * subtitle Avcodec.Packet.t ]

(** Reads the selected streams if any or all streams otherwise. *)
external read_input_packet : input container -> input_packet_result
  = "ocaml_av_read_input_packet"

(** Reads iteratively the selected streams if any or all streams otherwise. *)
let iter_input_packet ?(audio = fun _ _ -> ()) ?(video = fun _ _ -> ())
    ?(subtitle = fun _ _ -> ()) src =
  let rec iter () =
    match read_input_packet src with
      | `Audio (index, packet) ->
          audio index packet;
          iter ()
      | `Video (index, packet) ->
          video index packet;
          iter ()
      | `Subtitle (index, packet) ->
          subtitle index packet;
          iter ()
  in
  try iter () with Error `Eof -> ()

type input_frame_result =
  [ `Audio of int * audio frame
  | `Video of int * video frame
  | `Subtitle of int * subtitle frame ]

(** Reads the selected streams if any or all streams otherwise. *)
external read_input_frame : input container -> input_frame_result
  = "ocaml_av_read_input_frame"

(** Reads iteratively the selected streams if any or all streams otherwise. *)
let iter_input_frame ?(audio = fun _ _ -> ()) ?(video = fun _ _ -> ())
    ?(subtitle = fun _ _ -> ()) src =
  let rec iter () =
    match read_input_frame src with
      | `Audio (index, frame) ->
          audio index frame;
          iter ()
      | `Video (index, frame) ->
          video index frame;
          iter ()
      | `Subtitle (index, frame) ->
          subtitle index frame;
          iter ()
  in
  try iter () with Error `Eof -> ()

type seek_flag =
  | Seek_flag_backward
  | Seek_flag_byte
  | Seek_flag_any
  | Seek_flag_frame

external seek :
  (input, _) stream -> Time_format.t -> Int64.t -> seek_flag array -> unit
  = "ocaml_av_seek_frame"

(* Output *)
external open_output :
  ?format:(output, _) format ->
  string ->
  (string * string) array ->
  output container * string array = "ocaml_av_open_output"

let open_output ?format ?opts fname =
  let opts = opts_default opts in
  let ret, unused = open_output ?format fname (mk_opts opts) in
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
  let output, unused = ocaml_av_open_output_stream format avio (mk_opts opts) in
  filter_opts unused opts;
  Gc.finalise_last cleanup output;
  output

external output_started : output container -> bool = "ocaml_av_header_written"

external _set_metadata :
  output container -> int -> (string * string) array -> unit
  = "ocaml_av_set_metadata"

let set_output_metadata o tags = _set_metadata o (-1) (Array.of_list tags)
let set_metadata s tags = _set_metadata s.container s.index (Array.of_list tags)
let get_output s = s.container
let on_opt v fn = match v with None -> () | Some v -> fn v

let add_audio_opts ?channels ?channel_layout ~sample_rate ~sample_format
    ~time_base opts =
  Hashtbl.add opts "ar" (`Int sample_rate);
  on_opt channels (fun channels -> Hashtbl.add opts "ac" (`Int channels));
  on_opt channel_layout (fun channel_layout ->
      Hashtbl.add opts "channel_layout"
        (`Int (Channel_layout.get_id channel_layout)));
  Hashtbl.add opts "sample_fmt"
    (`Int (Avutil.Sample_format.get_id sample_format));
  Hashtbl.add opts "time_base" (`String (Avutil.string_of_rational time_base))

external new_audio_stream :
  output container ->
  int ->
  [ `Encoder ] Avcodec.Audio.t ->
  (string * string) array ->
  int * string array = "ocaml_av_new_audio_stream"

let new_audio_stream ?opts ?channels ?channel_layout ~sample_rate ~sample_format
    ~time_base ~codec container =
  let () =
    match (channels, channel_layout) with
      | None, None ->
          raise
            (Avutil.Error
               (`Failure
                 "At least one of channels or channel_layout must be passed!"))
      | _ -> ()
  in
  let opts = opts_default opts in
  add_audio_opts ?channels ?channel_layout ~sample_rate ~sample_format
    ~time_base opts;
  let ret, unused =
    new_audio_stream container
      (Sample_format.get_id sample_format)
      codec (mk_opts opts)
  in
  filter_opts unused opts;
  mk_stream container ret

let add_video_opts ?frame_rate ~pixel_format ~width ~height ~time_base opts =
  Hashtbl.add opts "pixel_format"
    (`String (Pixel_format.to_string pixel_format));
  Hashtbl.add opts "video_size" (`String (Printf.sprintf "%dx%d" width height));
  Hashtbl.add opts "time_base" (`String (Avutil.string_of_rational time_base));
  match frame_rate with
    | Some r -> Hashtbl.add opts "r" (`String (Avutil.string_of_rational r))
    | None -> ()

external new_video_stream :
  output container ->
  [ `Encoder ] Avcodec.Video.t ->
  (string * string) array ->
  int * string array = "ocaml_av_new_video_stream"

let new_video_stream ?opts ?frame_rate ~pixel_format ~width ~height ~time_base
    ~codec container =
  let opts = opts_default opts in
  add_video_opts ?frame_rate ~pixel_format ~width ~height ~time_base opts;
  let ret, unused = new_video_stream container codec (mk_opts opts) in
  filter_opts unused opts;
  mk_stream container ret

external new_subtitle_stream :
  output container ->
  [ `Encoder ] Avcodec.Subtitle.t ->
  (string * string) array ->
  int * string array = "ocaml_av_new_subtitle_stream"

let new_subtitle_stream ?opts ~time_base ~codec container =
  let opts = opts_default opts in
  Hashtbl.add opts "time_base" (`String (Avutil.string_of_rational time_base));
  let ret, unused = new_subtitle_stream container codec (mk_opts opts) in
  filter_opts unused opts;
  mk_stream container ret

external write_packet :
  (output, 'media) stream -> 'media Avcodec.Packet.t -> unit
  = "ocaml_av_write_stream_packet"

external write_frame : (output, 'media) stream -> 'media frame -> unit
  = "ocaml_av_write_stream_frame"

external write_audio_frame : output container -> audio frame -> unit
  = "ocaml_av_write_audio_frame"

external write_video_frame : output container -> video frame -> unit
  = "ocaml_av_write_video_frame"

external close : _ container -> unit = "ocaml_av_close"
