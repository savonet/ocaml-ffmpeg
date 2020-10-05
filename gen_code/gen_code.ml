external polymorphic_variant_string_to_c_value : string -> int64
  = "polymorphic_variant_string_to_c_value"

let if_d d fn = match d with None -> () | Some d -> fn d

let print_define_polymorphic_variant_value oc pv =
  let value = Int64.to_string (polymorphic_variant_string_to_c_value pv) in
  output_string oc ("#define PVV_" ^ pv ^ " (" ^ value ^ ")\n")

let rec find_line ic line_re =
  try
    if Str.string_match line_re (input_line ic) 0 then true
    else find_line ic line_re
  with End_of_file -> false

exception Found of string

let get_path filenames =
  try
    List.iter
      (fun filename ->
        List.iter
          (fun path ->
            let p = path ^ filename in
            if Sys.file_exists p then raise (Found p))
          Config.paths)
      filenames;
    None
  with Found p -> Some p

let rec id_to_pv_value id values =
  let id = if id.[0] >= '0' && id.[0] <= '9' then "_" ^ id else id in
  let id =
    String.(
      uppercase_ascii (sub id 0 1) ^ lowercase_ascii (sub id 1 (length id - 1)))
  in
  let value = polymorphic_variant_string_to_c_value id in

  if List.mem value values then id_to_pv_value (id ^ "_") values else (id, value)

let translate_enum_lines ?h_oc ?ml_oc ic labels =
  let ( start_pat,
        pat,
        end_pat,
        enum_prefix,
        c_type_name,
        c_fun_radix,
        ml_type_name ) =
    labels
  in

  let start_re = Str.regexp start_pat in
  let re = Str.regexp pat in
  let end_re = Str.regexp end_pat in

  let print_c words =
    if_d h_oc (fun oc -> output_string oc (String.concat "" words ^ "\n"))
  in

  let print_ml words =
    let line = String.concat "" words ^ "\n" in
    if_d ml_oc (fun oc -> output_string oc line)
  in

  let rec loop values =
    try
      match input_line ic with
        | line when end_pat <> "" && Str.string_match end_re line 0 -> values
        | line when Str.string_match re line 0 ->
            let id = Str.matched_group 1 line in
            let pv, value = id_to_pv_value id values in

            print_c
              ["  {("; Int64.to_string value; "), "; enum_prefix; id; "},"];
            print_ml ["  | `"; pv];

            loop (value :: values)
        | _ -> loop values
    with End_of_file -> values
  in

  if start_pat = "" || find_line ic start_re then (
    let tab_name = enum_prefix ^ String.uppercase_ascii ml_type_name ^ "_TAB" in
    let tab_len = tab_name ^ "_LEN" in

    print_c ["static const int64_t "; tab_name; "[][2] = {"];

    print_ml ["type "; ml_type_name; " = ["];

    let values = loop [] in

    print_c ["};\n\n#define "; tab_len; " "; string_of_int (List.length values)];

    print_c
      [
        c_type_name;
        " ";
        c_fun_radix;
        "_val(value v){\nint i;\nfor(i=0;i<";
        tab_len;
        ";i++){\nif(v==";
        tab_name;
        "[i][0])return ";
        tab_name;
        "[i][1];\n}\nreturn VALUE_NOT_FOUND;\n}";
      ];

    print_c
      [
        "value Val_";
        c_fun_radix;
        "(";
        c_type_name;
        " t){\nint i;\nfor(i=0;i<";
        tab_len;
        "; i++){\nif(t==";
        tab_name;
        "[i][1])return ";
        tab_name;
        "[i][0];\n}\nreturn VALUE_NOT_FOUND;\n}";
      ];

    print_ml ["]\n"] )

let translate_enums_opt ?h_oc ?ml_oc in_names enums_labels =
  match get_path in_names with
    | None ->
        Printf.eprintf "WARNING : None of the header files [%s] where found\n"
          (String.concat "; " (List.map (Printf.sprintf "%S") in_names))
    | Some path ->
        let ic = open_in path in

        if_d h_oc (fun oc ->
            output_string oc "#define VALUE_NOT_FOUND 0xFFFFFFF\n\n");

        List.iter
          (fun labels -> translate_enum_lines ic labels ?h_oc ?ml_oc)
          enums_labels;

        close_in ic

let translate_enums in_names out_name enums_labels = function
  | "ml" ->
      let ml_oc = open_out (out_name ^ ".ml") in
      translate_enums_opt ~ml_oc in_names enums_labels;
      close_out ml_oc
  | "h" ->
      let h_oc = open_out (out_name ^ "_stubs.h") in
      translate_enums_opt ~h_oc in_names enums_labels;
      close_out h_oc
  | _ -> assert false

let gen_polymorphic_variant_h () =
  let pvv_oc_h = open_out "polymorphic_variant_values_stubs.h" in

  List.iter
    (print_define_polymorphic_variant_value pvv_oc_h)
    [
      "Audio";
      "Audio_frame";
      "Audio_packet";
      "Video";
      "Video_frame";
      "Video_packet";
      "Subtitle";
      "Subtitle_frame";
      "Subtitle_packet";
      "Data";
      "Attachment";
      "Nb";
      "Packet";
      "Frame";
      "Ok";
      "Again";
      "Second";
      "Millisecond";
      "Microsecond";
      "Nanosecond";
      "Buffer";
      "Link";
      "Sink";
      (* Avfilter flags *)
      "Dynamic_inputs";
      "Dynamic_outputs";
      "Slice_threads";
      "Support_timeline_generic";
      "Support_timeline_internal";
      (* Options *)
      "Constant";
      "Flags";
      "Int";
      "Int64";
      "Float";
      "Double";
      "String";
      "Rational";
      "Binary";
      "Dict";
      "UInt64";
      "Image_size";
      "Pixel_fmt";
      "Sample_fmt";
      "Video_rate";
      "Duration";
      "Color";
      "Channel_layout";
      "Bool";
      "Encoding_param";
      "Decoding_param";
      "Audio_param";
      "Video_param";
      "Subtitle_param";
      "Export";
      "Readonly";
      "Bsf_param";
      "Runtime_param";
      "Filtering_param";
      "Deprecated";
      "Child_consts";
      (* Errors *)
      "Bsf_not_found";
      "Decoder_not_found";
      "Demuxer_not_found";
      "Encoder_not_found";
      "Eof";
      "Exit";
      "Filter_not_found";
      "Invalid_data";
      "Muxer_not_found";
      "Option_not_found";
      "Patch_welcome";
      "Protocol_not_found";
      "Stream_not_found";
      "Bug";
      "Eagain";
      "Unknown";
      "Experimental";
      "Other";
      "Failure";
    ];

  close_out pvv_oc_h

let gen_polymorphic_variant = function
  | "h" -> gen_polymorphic_variant_h ()
  | _ -> assert false

let gen_codec_id mode =
  (* translate_enums parameters : *)
  (* in_name out_name title (start_pat, pat, end_pat, enum_prefix, c_type_name, c_fun_radix, ml_type_name) *)
  translate_enums
    ["/libavcodec/codec_id.h"; "/libavcodec/avcodec.h"]
    "codec_id"
    [
      ( "[ \t]*AV_CODEC_ID_NONE",
        "[ \t]*AV_CODEC_ID_\\([A-Z0-9_]+\\)",
        "[ \t]*AV_CODEC_ID_FIRST_AUDIO",
        "AV_CODEC_ID_",
        "enum AVCodecID",
        "VideoCodecID",
        "video" );
      ( "",
        "[ \t]*AV_CODEC_ID_\\([A-Z0-9_]+\\)",
        "[ \t]*AV_CODEC_ID_FIRST_SUBTITLE",
        "AV_CODEC_ID_",
        "enum AVCodecID",
        "AudioCodecID",
        "audio" );
      ( "",
        "[ \t]*AV_CODEC_ID_\\([A-Z0-9_]+\\)",
        "[ \t]*AV_CODEC_ID_FIRST_UNKNOWN",
        "AV_CODEC_ID_",
        "enum AVCodecID",
        "SubtitleCodecID",
        "subtitle" );
    ]
    mode

let gen_pixel_format mode =
  translate_enums ["/libavutil/pixfmt.h"] "pixel_format"
    [
      ( "enum AVPixelFormat",
        "[ \t]*AV_PIX_FMT_\\([A-Z0-9_]+\\)",
        "[ \t]*AV_PIX_FMT_DRM_PRIME",
        "AV_PIX_FMT_",
        "enum AVPixelFormat",
        "PixelFormat",
        "t" );
    ]
    mode

let gen_channel_layout mode =
  translate_enums
    ["/libavutil/channel_layout.h"]
    "channel_layout"
    [
      ( "",
        "#define AV_CH_LAYOUT_\\([A-Z0-9_]+\\)",
        "",
        "AV_CH_LAYOUT_",
        "uint64_t",
        "ChannelLayout",
        "t" );
    ]
    mode

let gen_codec_capabilities mode =
  translate_enums
    ["/libavcodec/codec.h"; "/libavcodec/avcodec.h"]
    "codec_capabilities"
    [
      ( "",
        "#define AV_CODEC_CAP_\\([A-Z0-9_]+\\)",
        "",
        "AV_CODEC_CAP_",
        "uint64_t",
        "CodecCapabilities",
        "t" );
    ]
    mode

let gen_sample_format mode =
  translate_enums ["/libavutil/samplefmt.h"] "sample_format"
    [
      ( "enum AVSampleFormat",
        "[ \t]*AV_SAMPLE_FMT_\\([A-Z0-9_]+\\)",
        "[ \t]*AV_SAMPLE_FMT_NB",
        "AV_SAMPLE_FMT_",
        "enum AVSampleFormat",
        "SampleFormat",
        "t" );
    ]
    mode

let gen_swresample_options mode =
  translate_enums
    ["/libswresample/swresample.h"]
    "swresample_options"
    [
      ( "[ \t]*SWR_DITHER_NONE",
        "[ \t]*SWR_\\([A-Z0-9_]+\\)",
        "[ \t]*SWR_DITHER_NS",
        "SWR_",
        "enum SwrDitherType",
        "DitherType",
        "dither_type" );
      ( "enum SwrEngine",
        "[ \t]*SWR_\\([A-Z0-9_]+\\)",
        "[ \t]*SWR_ENGINE_NB",
        "SWR_",
        "enum SwrEngine",
        "Engine",
        "engine" );
      ( "enum SwrFilterType",
        "[ \t]*SWR_\\([A-Z0-9_]+\\)",
        "\\};",
        "SWR_",
        "enum SwrFilterType",
        "FilterType",
        "filter_type" );
    ]
    mode

let () =
  let mode = Sys.argv.(2) in
  match Sys.argv.(1) with
    | "polymorphic_variant" -> gen_polymorphic_variant mode
    | "codec_id" -> gen_codec_id mode
    | "pixel_format" -> gen_pixel_format mode
    | "channel_layout" -> gen_channel_layout mode
    | "sample_format" -> gen_sample_format mode
    | "swresample_options" -> gen_swresample_options mode
    | "codec_capabilities" -> gen_codec_capabilities mode
    | _ -> assert false
