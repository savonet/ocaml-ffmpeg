open Avutil

external init : unit -> unit = "ocaml_av_init" [@@noalloc]

let () = init ()

(* Format *)
module Format = struct
  external get_input_name : (input, _) format -> string
    = "ocaml_av_input_format_get_name"

  external get_input_long_name : (input, _) format -> string
    = "ocaml_av_input_format_get_long_name"

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

external finalize_av : _ container -> unit = "ocaml_av_finalize_av"

let () = Callback.register "ocaml_av_finalize_av" finalize_av

(* Input *)
external open_input : string -> input container = "ocaml_av_open_input"

external open_input_format : (input, _) format -> input container
  = "ocaml_av_open_input_format"

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

external ocaml_av_open_input_stream : avio -> input container
  = "ocaml_av_open_input_stream"

external caml_av_input_io_finalise : avio -> unit = "caml_av_input_io_finalise"

let open_input_stream ?seek read =
  let avio = ocaml_av_create_read_io 4096 ?seek read in
  let cleanup () = caml_av_input_io_finalise avio in
  let input = ocaml_av_open_input_stream avio in
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

external reuse_output : input container -> bool -> unit
  = "ocaml_av_reuse_output"

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

external _set_metadata :
  output container -> int -> (string * string) array -> unit
  = "ocaml_av_set_metadata"

let set_output_metadata o tags = _set_metadata o (-1) (Array.of_list tags)
let set_metadata s tags = _set_metadata s.container s.index (Array.of_list tags)
let get_output s = s.container
let on_opt v fn = match v with None -> () | Some v -> fn v

let mk_base_opts ?bit_rate ?time_base () =
  let opts = Hashtbl.create 0 in
  on_opt bit_rate (fun b -> Hashtbl.add opts "b" (`Int b));
  on_opt time_base (fun { num; den } ->
      Hashtbl.add opts "time_base" (`String (Printf.sprintf "%d/%d" num den)));
  opts

let mk_audio_opts ?channels ?channel_layout ?bit_rate ?sample_rate
    ?sample_format ?time_base ?params ?stream () =
  let if_opt v d = match v with None -> Some d | Some _ -> v in
  let params =
    match (params, stream) with
      | Some _, _ -> params
      | None, Some s -> Some (get_codec_params s)
      | _ -> None
  in
  let channels, channel_layout, bit_rate, sample_rate, sample_format =
    match params with
      | None -> (channels, channel_layout, bit_rate, sample_rate, sample_format)
      | Some p ->
          ( if_opt channels (Avcodec.Audio.get_nb_channels p),
            if_opt channel_layout (Avcodec.Audio.get_channel_layout p),
            if_opt bit_rate (Avcodec.Audio.get_bit_rate p),
            if_opt sample_rate (Avcodec.Audio.get_sample_rate p),
            if_opt sample_format (Avcodec.Audio.get_sample_format p) )
  in
  let sample_rate = match sample_rate with Some r -> r | None -> 44100 in
  let channels =
    match channels with
      | Some c -> c
      | None -> (
          match channel_layout with
            | Some l -> Avutil.Channel_layout.get_nb_channels l
            | None -> 2 )
  in
  let channel_layout =
    match channel_layout with
      | Some cl -> cl
      | None -> (
          try Avutil.Channel_layout.get_default channels
          with Not_found ->
            raise
              (Error
                 (`Failure
                   (Printf.sprintf
                      "Could not find a suitable channel layout for %d channels"
                      channels))) )
  in
  let time_base = if_opt time_base { num = 1; den = sample_rate } in
  let opts = mk_base_opts ?bit_rate ?time_base () in
  Hashtbl.add opts "ar" (`Int sample_rate);
  Hashtbl.add opts "ac" (`Int channels);
  Hashtbl.add opts "channel_layout"
    (`Int (Channel_layout.get_id channel_layout));
  on_opt bit_rate (fun br -> Hashtbl.add opts "b" (`Int br));
  on_opt sample_format (fun sf ->
      Hashtbl.add opts "sample_fmt" (`Int (Avutil.Sample_format.get_id sf)));
  opts

external new_audio_stream :
  output container ->
  int ->
  [ `Encoder ] Avcodec.Audio.t ->
  (string * string) array ->
  int * string array = "ocaml_av_new_audio_stream"

let new_audio_stream ?opts ~codec container =
  let opts = match opts with Some opts -> opts | None -> mk_audio_opts () in
  let sample_format =
    try
      let ret = Hashtbl.find opts "sample_fmt" in
      Hashtbl.filter_map_inplace
        (fun k v -> if k = "sample_fmt" then None else Some v)
        opts;
      match ret with
        | `String name -> Avutil.Sample_format.find name
        | `Int id -> Avutil.Sample_format.find_id id
        | `Float id -> Avutil.Sample_format.find_id (int_of_float id)
    with Not_found -> Avcodec.Audio.find_best_sample_format codec `Dbl
  in
  let ret, unused =
    new_audio_stream container
      (Sample_format.get_id sample_format)
      codec (mk_opts opts)
  in
  filter_opts unused opts;
  mk_stream container ret

let mk_video_opts ?pixel_format ?size ?bit_rate ?frame_rate ?time_base ?params
    ?stream () =
  let if_opt v d = match v with None -> Some d | Some _ -> v in
  let params =
    match (params, stream) with
      | Some _, _ -> params
      | None, Some s -> Some (get_codec_params s)
      | _ -> None
  in
  let pixel_format, bit_rate, size =
    match params with
      | None -> (pixel_format, bit_rate, size)
      | Some p ->
          ( if_opt pixel_format (Avcodec.Video.get_pixel_format p),
            if_opt bit_rate (Avcodec.Video.get_bit_rate p),
            if_opt size (Avcodec.Video.get_width p, Avcodec.Video.get_height p)
          )
  in
  let frame_rate = match frame_rate with Some r -> r | None -> 25 in
  let time_base = if_opt time_base { num = 1; den = 25 } in
  let opts = mk_base_opts ?bit_rate ?time_base () in
  Hashtbl.add opts "r" (`Int frame_rate);
  on_opt pixel_format (fun pf ->
      Hashtbl.add opts "pixel_format" (`String (Pixel_format.to_string pf)));
  on_opt bit_rate (fun br -> Hashtbl.add opts "b" (`Int br));
  on_opt size (fun (w, h) ->
      Hashtbl.add opts "video_size" (`String (Printf.sprintf "%dx%d" w h)));
  opts

external new_video_stream :
  output container ->
  [ `Encoder ] Avcodec.Video.t ->
  (string * string) array ->
  int * string array = "ocaml_av_new_video_stream"

let new_video_stream ?opts ~codec container =
  let opts = match opts with Some opts -> opts | None -> mk_audio_opts () in
  let ret, unused = new_video_stream container codec (mk_opts opts) in
  filter_opts unused opts;
  mk_stream container ret

let mk_subtitle_opts ?time_base () = mk_base_opts ?time_base ()

external new_subtitle_stream :
  output container ->
  [ `Encoder ] Avcodec.Subtitle.t ->
  (string * string) array ->
  int * string array = "ocaml_av_new_subtitle_stream"

let new_subtitle_stream ?opts ~codec container =
  let opts = opts_default opts in
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
