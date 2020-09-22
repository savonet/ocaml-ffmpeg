open Avfilter

let () = Printexc.record_backtrace true

let string_of_pad { audio; video } =
  let audio =
    String.concat "\n"
      (List.map
         (fun pad -> Printf.sprintf "- name: %s, type: audio" (pad_name pad))
         audio)
  in

  let video =
    String.concat "\n"
      (List.map
         (fun pad -> Printf.sprintf "- name: %s, type: video" (pad_name pad))
         video)
  in

  let audio = if audio = "" then [] else ["\n"; audio] in
  let video = if video = "" then [] else ["\n"; video] in

  String.concat "" (audio @ video)

let string_of_spec to_string { Avutil.Options.default; min; max; values } =
  let opt_str = function None -> "none" | Some v -> to_string v in
  Printf.sprintf "{default: %s, min: %s, max: %s, values: %s}" (opt_str default)
    (opt_str min) (opt_str max)
    (Printf.sprintf "[%s]"
       (String.concat ", "
          (List.map
             (fun (name, v) -> Printf.sprintf "%s: %s" name (to_string v))
             values)))

let string_of_flags flags =
  let string_of_flag = function
    | `Encoding_param -> "encoding param"
    | `Decoding_param -> "decoding param"
    | `Audio_param -> "audio param"
    | `Video_param -> "video param"
    | `Subtitle_param -> "subtitle param"
    | `Export -> "export"
    | `Readonly -> "readonly"
    | `Bsf_param -> "bsf param"
    | `Runtime_param -> "runtime param"
    | `Filtering_param -> "filtering param"
    | `Deprecated -> "deprecated"
    | `Child_consts -> "child constants"
  in
  String.concat ", " (List.map string_of_flag flags)

let string_of_option { Avutil.Options.name; help; flags; spec } =
  let _type, spec =
    match spec with
      | `Flags entry -> ("flags", string_of_spec Int64.to_string entry)
      | `Int entry -> ("int", string_of_spec string_of_int entry)
      | `Int64 entry -> ("int64", string_of_spec Int64.to_string entry)
      | `Float entry -> ("float", string_of_spec string_of_float entry)
      | `Double entry -> ("double", string_of_spec string_of_float entry)
      | `String entry -> ("string", string_of_spec (fun v -> v) entry)
      | `Rational entry ->
          ( "rational",
            string_of_spec
              (fun { Avutil.num; den } -> Printf.sprintf "%d/%d" num den)
              entry )
      | `Binary entry -> ("binary", string_of_spec (fun v -> v) entry)
      | `Dict entry -> ("dict", string_of_spec (fun v -> v) entry)
      | `UInt64 entry -> ("uint64", string_of_spec Int64.to_string entry)
      | `Image_size entry -> ("image_size", string_of_spec (fun v -> v) entry)
      | `Pixel_fmt entry ->
          ("pixel_fmt", string_of_spec Avutil.Pixel_format.to_string entry)
      | `Sample_fmt entry ->
          ("sample_fmt", string_of_spec Avutil.Sample_format.get_name entry)
      | `Video_rate entry -> ("video_rate", string_of_spec (fun v -> v) entry)
      | `Duration entry -> ("duration", string_of_spec Int64.to_string entry)
      | `Color entry -> ("color", string_of_spec (fun v -> v) entry)
      | `Channel_layout entry ->
          ( "channel_layout",
            string_of_spec Avutil.Channel_layout.get_description entry )
      | `Bool entry -> ("bool", string_of_spec string_of_bool entry)
  in

  Printf.sprintf
    "- %s:\n    type: %s\n    help: %s\n    flags: %s\n    spec: %s" name _type
    (match help with None -> "none" | Some v -> v)
    (string_of_flags flags) spec

let () =
  let print_filters cat =
    List.iter (fun { name; description; options; io } ->
        let { inputs; outputs } = io in
        let options = Avutil.Options.opts options in
        Printf.printf
          "%s name: %s\n\
           description: %s\n\
           options:\n\
           %s\n\
           inputs:%s\n\
           outputs:%s\n\n"
          cat name description
          (String.concat "\n" (List.map string_of_option options))
          (string_of_pad inputs) (string_of_pad outputs))
  in

  Printf.printf "## Buffers (inputs) ##\n\n";
  print_filters "Buffer" [abuffer];
  print_filters "Buffer" [buffer];

  Printf.printf "## Sinks (oututs):\n\n";
  print_filters "Sink" [abuffersink];
  print_filters "Sink" [buffersink];

  Printf.printf "## Filters ##\n\n";
  print_filters "Filter" filters;

  Gc.full_major ()
