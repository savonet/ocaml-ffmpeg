(* Line *)
type input
type output

(* Container *)
type 'a container

(** {1 Media types} *)

type audio = [ `Audio ]
type video = [ `Video ]
type subtitle = [ `Subtitle ]

type media_type =
  [ audio | video | subtitle | `Unknown | `Data | `Attachment | `Nb ]

(* Format *)
type ('line, 'media) format

(* Frame *)
type 'media frame

external frame_pts : _ frame -> Int64.t option = "ocaml_avutil_frame_pts"

external frame_set_pts : _ frame -> Int64.t option -> unit
  = "ocaml_avutil_frame_set_pts"

external frame_pkt_duration : _ frame -> Int64.t option
  = "ocaml_avutil_frame_pkt_duration"

external frame_copy : 'a frame -> 'b frame -> unit = "ocaml_avutil_frame_copy"

type error =
  [ `Bsf_not_found
  | `Decoder_not_found
  | `Demuxer_not_found
  | `Encoder_not_found
  | `Eof
  | `Exit
  | `Filter_not_found
  | `Invalid_data
  | `Muxer_not_found
  | `Option_not_found
  | `Patch_welcome
  | `Protocol_not_found
  | `Stream_not_found
  | `Bug
  | `Eagain
  | `Unknown
  | `Experimental
  | `Other of int
  | `Failure of string ]

external string_of_error : error -> string = "ocaml_avutil_string_of_error"

exception Error of error

let () =
  Printexc.register_printer (function
    | Error err ->
        Some (Printf.sprintf "FFmpeg.Avutils.Error(%s)" (string_of_error err))
    | _ -> None)

let () =
  Callback.register_exception "ffmpeg_exn_error" (Error `Unknown);
  Callback.register "ffmpeg_exn_failure" (fun s -> raise (Error (`Failure s)));
  Callback.register "ffmpeg_gc_finalise" Gc.finalise

external ocaml_avutil_register_lock_manager : unit -> bool
  = "ocaml_avutil_register_lock_manager"
  [@@noalloc]

let () =
  if not (ocaml_avutil_register_lock_manager ()) then
    raise (Error (`Failure "Failed to register lock manager"))

type data =
  (int, Bigarray.int8_unsigned_elt, Bigarray.c_layout) Bigarray.Array1.t

let create_data len =
  Bigarray.Array1.create Bigarray.int8_unsigned Bigarray.c_layout len

type rational = { num : int; den : int }

external av_d2q : float -> rational = "ocaml_avutil_av_d2q"

let string_of_rational { num; den } = Printf.sprintf "%d/%d" num den

external time_base : unit -> rational = "ocaml_avutil_time_base"

module Time_format = struct
  type t = [ `Second | `Millisecond | `Microsecond | `Nanosecond ]
end

module Log = struct
  type level =
    [ `Quiet | `Panic | `Fatal | `Error | `Warning | `Info | `Verbose | `Debug ]

  let int_of_level = function
    | `Quiet -> -8
    | `Panic -> 0
    | `Fatal -> 8
    | `Error -> 16
    | `Warning -> 24
    | `Info -> 32
    | `Verbose -> 40
    | `Debug -> 48

  external set_level : int -> unit = "ocaml_avutil_set_log_level"

  let set_level level = set_level (int_of_level level)

  external setup_log_callback : unit -> unit = "ocaml_avutil_setup_log_callback"
  external process_log : (string -> unit) -> unit = "ocaml_ffmpeg_process_log"

  let log_fn = ref (Printf.printf "%s")
  let log_fn_m = Mutex.create ()

  let set_callback fn =
    setup_log_callback ();
    Mutex.lock log_fn_m;
    log_fn := fn;
    Mutex.unlock log_fn_m

  external clear_callback : unit -> unit = "ocaml_avutil_clear_log_callback"

  let clear_callback () =
    clear_callback ();
    set_callback (Printf.printf "%s")

  let () =
    ignore (Thread.create (fun () -> process_log (fun msg -> !log_fn msg)) ())
end

module Pixel_format = struct
  type t = Pixel_format.t
  type flag = Pixel_format_flag.t

  type component_descriptor = {
    plane : int;
    step : int;
    offset : int;
    shift : int;
    depth : int;
  }

  (* An extra hidden field is stored on the C side
     with a reference to the underlying C descriptor
     for use with the C functions consuming it. *)
  type descriptor = {
    name : string;
    nb_components : int;
    log2_chroma_w : int;
    log2_chroma_h : int;
    flags : flag list;
    comp : component_descriptor list;
    alias : string option;
  }

  external descriptor : t -> descriptor = "ocaml_avutil_pixelformat_descriptor"
  external bits : descriptor -> int = "ocaml_avutil_pixelformat_bits_per_pixel"
  external planes : t -> int = "ocaml_avutil_pixelformat_planes"
  external to_string : t -> string option = "ocaml_avutil_pixelformat_to_string"
  external of_string : string -> t = "ocaml_avutil_pixelformat_of_string"
  external get_id : t -> int = "ocaml_avutil_get_pixel_fmt_id"
  external find_id : int -> t = "ocaml_avutil_find_pixel_fmt_from_id"
end

module Channel_layout = struct
  type t = Channel_layout.t

  external get_description : t -> int -> string
    = "ocaml_avutil_get_channel_layout_description"

  let get_description ?(channels = -1) ch = get_description ch channels

  external find : string -> t = "ocaml_avutil_get_channel_layout"

  external get_nb_channels : t -> int
    = "ocaml_avutil_get_channel_layout_nb_channels"

  external get_default : int -> t = "ocaml_avutil_get_default_channel_layout"
  external get_id : t -> int64 = "ocaml_avutil_get_channel_layout_id"
  external from_id : int64 -> t = "ocaml_avutil_channel_layout_of_id"
end

module Sample_format = struct
  type t = Sample_format.t

  external get_name : t -> string option = "ocaml_avutil_get_sample_fmt_name"
  external get_id : t -> int = "ocaml_avutil_get_sample_fmt_id"
  external find : string -> t = "ocaml_avutil_find_sample_fmt"
  external find_id : int -> t = "ocaml_avutil_find_sample_fmt_from_id"
end

module Audio = struct
  external create_frame :
    Sample_format.t -> Channel_layout.t -> int -> int -> audio frame
    = "ocaml_avutil_audio_create_frame"

  external frame_get_sample_format : audio frame -> Sample_format.t
    = "ocaml_avutil_audio_frame_get_sample_format"

  external frame_get_sample_rate : audio frame -> int
    = "ocaml_avutil_audio_frame_get_sample_rate"

  external frame_get_channels : audio frame -> int
    = "ocaml_avutil_audio_frame_get_channels"

  external frame_get_channel_layout : audio frame -> Channel_layout.t
    = "ocaml_avutil_audio_frame_get_channel_layout"

  external frame_nb_samples : audio frame -> int
    = "ocaml_avutil_audio_frame_nb_samples"

  external frame_copy_samples :
    audio frame -> int -> audio frame -> int -> int -> unit
    = "ocaml_avutil_audio_frame_copy_samples"
end

module Video = struct
  type planes = (data * int) array

  external create_frame : int -> int -> Pixel_format.t -> video frame
    = "ocaml_avutil_video_create_frame"

  external frame_get_linesize : video frame -> int -> int
    = "ocaml_avutil_video_frame_get_linesize"

  external get_frame_planes : video frame -> bool -> planes
    = "ocaml_avutil_video_get_frame_bigarray_planes"

  let frame_visit ~make_writable visit frame =
    visit (get_frame_planes frame make_writable);
    frame

  external frame_get_width : video frame -> int
    = "ocaml_avutil_video_frame_width"

  external frame_get_height : video frame -> int
    = "ocaml_avutil_video_frame_height"

  external frame_get_pixel_format : video frame -> Pixel_format.t
    = "ocaml_avutil_video_frame_get_pixel_format"
end

module Subtitle = struct
  let time_base () = { num = 1; den = 100 }

  external create_frame : int64 -> int64 -> string array -> subtitle frame
    = "ocaml_avutil_subtitle_create_frame"

  let create_frame start_time end_time lines =
    let num_time_base = float_of_int (time_base ()).num in
    let den_time_base = float_of_int (time_base ()).den in

    create_frame
      (Int64.of_float (start_time *. den_time_base /. num_time_base))
      (Int64.of_float (end_time *. den_time_base /. num_time_base))
      (Array.of_list lines)

  external frame_to_lines : subtitle frame -> int64 * int64 * string array
    = "ocaml_avutil_subtitle_to_lines"

  let frame_to_lines t =
    let num_time_base = float_of_int (time_base ()).num in
    let den_time_base = float_of_int (time_base ()).den in

    let s, e, lines = frame_to_lines t in

    Int64.
      ( to_float s *. num_time_base /. den_time_base,
        to_float e *. num_time_base /. den_time_base,
        Array.to_list lines )
end

module Options = struct
  type t

  type 'a entry = {
    default : 'a option;
    min : 'a option;
    max : 'a option;
    values : (string * 'a) list;
  }

  type flag =
    [ `Encoding_param
    | `Decoding_param
    | `Audio_param
    | `Video_param
    | `Subtitle_param
    | `Export
    | `Readonly
    | `Bsf_param
    | `Runtime_param
    | `Filtering_param
    | `Deprecated
    | `Child_consts ]

  external int_of_flag : flag -> int = "ocaml_avutil_av_opt_int_of_flag"

  let flags_of_flags _flags =
    List.fold_left
      (fun flags flag ->
        if _flags land int_of_flag flag = 0 then flags else flag :: flags)
      []
      [
        `Encoding_param;
        `Decoding_param;
        `Audio_param;
        `Video_param;
        `Subtitle_param;
        `Export;
        `Readonly;
        `Bsf_param;
        `Runtime_param;
        `Filtering_param;
        `Deprecated;
        `Child_consts;
      ]

  type spec =
    [ `Flags of int64 entry
    | `Int of int entry
    | `Int64 of int64 entry
    | `Float of float entry
    | `Double of float entry
    | `String of string entry
    | `Rational of rational entry
    | `Binary of string entry
    | `Dict of string entry
    | `UInt64 of int64 entry
    | `Image_size of string entry
    | `Pixel_fmt of Pixel_format.t entry
    | `Sample_fmt of Sample_format.t entry
    | `Video_rate of string entry
    | `Duration of int64 entry
    | `Color of string entry
    | `Channel_layout of Channel_layout.t entry
    | `Bool of bool entry ]

  type opt = {
    name : string;
    help : string option;
    flags : flag list;
    spec : spec;
  }

  type 'a _entry = { _default : 'a option; _min : 'a option; _max : 'a option }
  type constant

  external default_int64 : constant -> int64
    = "ocaml_avutil_avopt_default_int64"

  external default_double : constant -> float
    = "ocaml_avutil_avopt_default_double"

  external default_string : constant -> string
    = "ocaml_avutil_avopt_default_string"

  type _spec =
    [ `Constant of constant _entry
    | `Flags of int64 _entry
    | `Int of int _entry
    | `Int64 of int64 _entry
    | `Float of float _entry
    | `Double of float _entry
    | `String of string _entry
    | `Rational of rational _entry
    | `Binary of string _entry
    | `Dict of string _entry
    | `UInt64 of int64 _entry
    | `Image_size of string _entry
    | `Pixel_fmt of Pixel_format.t _entry
    | `Sample_fmt of Sample_format.t _entry
    | `Video_rate of string _entry
    | `Duration of int64 _entry
    | `Color of string _entry
    | `Channel_layout of Channel_layout.t _entry
    | `Bool of bool _entry ]

  type _opt_cursor
  type _class_cursor
  type _cursor = { _opt_cursor : _opt_cursor; _class_cursor : _class_cursor }

  type _opt = {
    _name : string;
    _help : string option;
    _spec : _spec;
    _flags : int;
    _unit : string option;
    _cursor : _cursor option;
  }

  external av_opt_next : _cursor option -> t -> _opt option
    = "ocaml_avutil_av_opt_next"

  let constant_of_opt opt (name, { _default; _ }) =
    let append fn l = (name, fn (Option.get _default)) :: l in

    let spec =
      (* See: https://ffmpeg.org/doxygen/trunk/opt_8c_source.html#l01281 *)
      match opt.spec with
        (* Int *)
        | `Flags ({ values; _ } as spec) ->
            `Int64 { spec with values = append default_int64 values }
        | `Int ({ values; _ } as spec) ->
            `Int
              {
                spec with
                values = append (fun v -> Int64.to_int (default_int64 v)) values;
              }
        | `Int64 ({ values; _ } as spec) ->
            `Int64 { spec with values = append default_int64 values }
        | `UInt64 ({ values; _ } as spec) ->
            `UInt64 { spec with values = append default_int64 values }
        | `Duration ({ values; _ } as spec) ->
            `Duration { spec with values = append default_int64 values }
        | `Bool ({ values; _ } as spec) ->
            `Bool
              {
                spec with
                values = append (fun v -> default_int64 v = 0L) values;
              }
        (* Float *)
        | `Float ({ values; _ } as spec) ->
            `Float { spec with values = append default_double values }
        | `Double ({ values; _ } as spec) ->
            `Double { spec with values = append default_double values }
        (* Rational *)
        (* This is surprising but this is the current implementation
           it looks like. Might be historical. *)
        | `Rational ({ values; _ } as spec) ->
            `Rational
              {
                spec with
                values = append (fun v -> av_d2q (default_double v)) values;
              }
        (* String *)
        | `String ({ values; _ } as spec) ->
            `String { spec with values = append default_string values }
        | `Video_rate ({ values; _ } as spec) ->
            `Video_rate { spec with values = append default_string values }
        | `Color ({ values; _ } as spec) ->
            `Color { spec with values = append default_string values }
        | `Image_size ({ values; _ } as spec) ->
            `Image_size { spec with values = append default_string values }
        | `Dict ({ values; _ } as spec) ->
            `Dict { spec with values = append default_string values }
        (* Other *)
        | `Channel_layout ({ values; _ } as spec) ->
            `Channel_layout
              {
                spec with
                values =
                  append
                    (fun v -> Channel_layout.from_id (default_int64 v))
                    values;
              }
        | `Sample_fmt ({ values; _ } as spec) ->
            `Sample_fmt
              {
                spec with
                values =
                  append
                    (fun v ->
                      Sample_format.find_id (Int64.to_int (default_int64 v)))
                    values;
              }
        | `Pixel_fmt ({ values; _ } as spec) ->
            `Pixel_fmt
              {
                spec with
                values =
                  append
                    (fun v ->
                      Pixel_format.find_id (Int64.to_int (default_int64 v)))
                    values;
              }
        | _ -> failwith "Incompatible constant!"
    in
    { opt with spec }

  let opts v =
    let constants = Hashtbl.create 10 in

    let opt_of_opt { _name; _help; _spec; _flags; _unit; _ } =
      let spec =
        match _spec with
          | `Flags { _default; _min; _max } ->
              `Flags { default = _default; min = _min; max = _max; values = [] }
          | `Int { _default; _min; _max; _ } ->
              `Int { default = _default; min = _min; max = _max; values = [] }
          | `Int64 { _default; _min; _max; _ } ->
              `Int64 { default = _default; min = _min; max = _max; values = [] }
          | `Float { _default; _min; _max; _ } ->
              `Float { default = _default; min = _min; max = _max; values = [] }
          | `Double { _default; _min; _max; _ } ->
              `Double
                { default = _default; min = _min; max = _max; values = [] }
          | `String { _default; _min; _max; _ } ->
              `String
                { default = _default; min = _min; max = _max; values = [] }
          | `Rational { _default; _min; _max; _ } ->
              `Rational
                { default = _default; min = _min; max = _max; values = [] }
          | `Binary { _default; _min; _max; _ } ->
              `Binary
                { default = _default; min = _min; max = _max; values = [] }
          | `Dict { _default; _min; _max; _ } ->
              `Dict { default = _default; min = _min; max = _max; values = [] }
          | `UInt64 { _default; _min; _max; _ } ->
              `UInt64
                { default = _default; min = _min; max = _max; values = [] }
          | `Image_size { _default; _min; _max; _ } ->
              `Image_size
                { default = _default; min = _min; max = _max; values = [] }
          | `Pixel_fmt { _default; _min; _max; _ } ->
              `Pixel_fmt
                { default = _default; min = _min; max = _max; values = [] }
          | `Sample_fmt { _default; _min; _max; _ } ->
              `Sample_fmt
                { default = _default; min = _min; max = _max; values = [] }
          | `Video_rate { _default; _min; _max; _ } ->
              `Video_rate
                { default = _default; min = _min; max = _max; values = [] }
          | `Duration { _default; _min; _max; _ } ->
              `Duration
                { default = _default; min = _min; max = _max; values = [] }
          | `Color { _default; _min; _max; _ } ->
              `Color { default = _default; min = _min; max = _max; values = [] }
          | `Channel_layout { _default; _min; _max; _ } ->
              `Channel_layout
                { default = _default; min = _min; max = _max; values = [] }
          | `Bool { _default; _min; _max; _ } ->
              `Bool { default = _default; min = _min; max = _max; values = [] }
          | `Constant _ -> assert false
      in
      let opt =
        { name = _name; help = _help; flags = flags_of_flags _flags; spec }
      in
      match _unit with
        | Some u when Hashtbl.mem constants u ->
            List.fold_left constant_of_opt opt (Hashtbl.find_all constants u)
        | _ -> opt
    in

    let rec f _cursor _opts =
      match av_opt_next _cursor v with
        | None -> List.map opt_of_opt _opts
        | Some { _name; _spec = `Constant s; _cursor; _unit; _ } ->
            Hashtbl.add constants (Option.get _unit) (_name, s);
            f _cursor _opts
        | Some _opt -> f _opt._cursor (_opt :: _opts)
    in

    f None []
end

(* Options *)
type value =
  [ `String of string | `Int of int | `Int64 of int64 | `Float of float ]

type opts = (string, value) Hashtbl.t

let _opt_val = function
  | `String s -> s
  | `Int i -> string_of_int i
  | `Int64 i -> Int64.to_string i
  | `Float f -> string_of_float f

let opts_default = function None -> Hashtbl.create 0 | Some opts -> opts

let mk_opts_array opts =
  Array.of_list
    (Hashtbl.fold
       (fun opt_name opt_val cur -> (opt_name, _opt_val opt_val) :: cur)
       opts [])

let on_opt v fn = match v with None -> () | Some v -> fn v

let add_audio_opts ?channels ?channel_layout ~sample_rate ~sample_format
    ~time_base opts =
  Hashtbl.add opts "ar" (`Int sample_rate);
  on_opt channels (fun channels -> Hashtbl.add opts "ac" (`Int channels));
  on_opt channel_layout (fun channel_layout ->
      Hashtbl.add opts "channel_layout"
        (`Int64 (Channel_layout.get_id channel_layout)));
  Hashtbl.add opts "sample_fmt" (`Int (Sample_format.get_id sample_format));
  Hashtbl.add opts "time_base" (`String (string_of_rational time_base))

let mk_audio_opts ?opts ?channels ?channel_layout ~sample_rate ~sample_format
    ~time_base =
  let () =
    match (channels, channel_layout) with
      | None, None ->
          raise
            (Error
               (`Failure
                 "At least one of channels or channel_layout must be passed!"))
      | _ -> ()
  in
  let opts = opts_default opts in
  add_audio_opts ?channels ?channel_layout ~sample_rate ~sample_format
    ~time_base opts;
  opts

let add_video_opts ?frame_rate ~pixel_format ~width ~height ~time_base opts =
  Hashtbl.add opts "pixel_format" (`Int (Pixel_format.get_id pixel_format));
  Hashtbl.add opts "video_size" (`String (Printf.sprintf "%dx%d" width height));
  Hashtbl.add opts "time_base" (`String (string_of_rational time_base));
  match frame_rate with
    | Some r -> Hashtbl.add opts "r" (`String (string_of_rational r))
    | None -> ()

let mk_video_opts ?opts ?frame_rate ~pixel_format ~width ~height ~time_base =
  let opts = opts_default opts in
  add_video_opts ?frame_rate ~pixel_format ~width ~height ~time_base opts;
  opts

let filter_opts unused opts =
  Hashtbl.filter_map_inplace
    (fun k v -> if Array.mem k unused then Some v else None)
    opts
