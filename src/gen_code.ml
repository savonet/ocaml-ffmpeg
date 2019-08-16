
external polymorphic_variant_string_to_c_value : string -> int64 = "polymorphic_variant_string_to_c_value"


let print_define_polymorphic_variant_value oc pv =
  let value = Int64.to_string(polymorphic_variant_string_to_c_value pv) in
  output_string oc ("#define PVV_" ^ pv ^ " (" ^ value ^ ")\n")


let rec find_line ic line_re = try
    if Str.string_match line_re (input_line ic) 0 then true
    else find_line ic line_re
  with End_of_file -> false


let get_path filename =
  ("-I/usr/include"::"-I/usr/include/i386-linux-gnu"::(Sys.argv |> Array.to_list))
  |> List.fold_left(fun path param ->
      if path = None && "-I" = String.sub param 0 2 then (
        let p = (String.sub param 2 (String.length param - 2)) ^ filename in
        if Sys.file_exists p then Some p
        else (
          None
        )
      )
      else path
    ) None


let rec id_to_pv_value id values =
  let id = if id.[0] >= '0' && id.[0] <= '9' then "_" ^ id else id in
  let id = String.(uppercase(sub id 0 1) ^ lowercase(sub id 1 (length id - 1))) in
  let value = polymorphic_variant_string_to_c_value id in

  if List.mem value values then
    id_to_pv_value(id ^ "_") values
  else (id, value)


let translate_enum_lines ic labels c_oc ml_oc mli_oc =

  let start_pat, pat, end_pat, enum_prefix, c_type_name, c_fun_radix, ml_type_name = labels in

  let start_re = Str.regexp start_pat in
  let re = Str.regexp pat in
  let end_re = Str.regexp end_pat in

  let print_c words = output_string c_oc (String.concat "" words ^ "\n") in

  let print_ml words =
    let line = String.concat "" words ^ "\n" in
    output_string ml_oc line;
    output_string mli_oc line;
  in

  let rec loop values = try
      match input_line ic with
      | line when end_pat <> "" && Str.string_match end_re line 0 -> values
      | line when Str.string_match re line 0 ->
        let id = Str.matched_group 1 line in
        let pv, value = id_to_pv_value id values in

        print_c["  {("; Int64.to_string value; "), "; enum_prefix; id; "},"];
        print_ml["  | `"; pv];

        loop (value::values)
      | _ -> loop values
    with End_of_file -> values
  in

  if start_pat = "" || find_line ic start_re then (

    let tab_name = enum_prefix ^ String.uppercase ml_type_name ^ "_TAB" in
    let tab_len = tab_name ^ "_LEN" in

    print_c["static const int64_t "; tab_name; "[][2] = {"];


    print_ml["type "; ml_type_name; " = ["];

    let values = loop [] in

    print_c["};\n\n#define "; tab_len; " "; string_of_int(List.length values)];

    print_c[c_type_name;" ";c_fun_radix;"_val(value v){\nint i;\nfor(i=0;i<";tab_len;";i++){\nif(v==";tab_name;"[i][0])return ";tab_name;"[i][1];\n}\nreturn VALUE_NOT_FOUND;\n}"];

    print_c["value Val_";c_fun_radix;"(";c_type_name;" t){\nint i;\nfor(i=0;i<";tab_len;"; i++){\nif(t==";tab_name;"[i][1])return ";tab_name;"[i][0];\n}\nreturn VALUE_NOT_FOUND;\n}"];

    print_ml["]\n"];
  )


let translate_enums in_name out_name title enums_labels =

  match get_path in_name with
  | None -> Printf.eprintf"WARNING : header file %s not found\n" in_name
  | Some path ->
    let ic = open_in path in

    let c_oc = open_out (out_name^"_stubs.h") in
    let ml_oc = open_out (out_name^".ml") in
    let mli_oc = open_out (out_name^".mli") in

    output_string mli_oc ("(** " ^ title ^ " *)\n\n");
    output_string c_oc "#define VALUE_NOT_FOUND 0xFFFFFFF\n\n";

    List.iter(fun labels -> translate_enum_lines ic labels c_oc ml_oc mli_oc) enums_labels;

    close_in ic;
    close_out c_oc;
    close_out ml_oc;
    close_out mli_oc


let () =
  let pvv_oc = open_out "polymorphic_variant_values_stubs.h" in

  List.iter(print_define_polymorphic_variant_value pvv_oc)
    ["Audio"; "Video"; "Subtitle"; "Packet"; "Frame";
     "Ok"; "Again"; "Second"; "Millisecond"; "Microsecond"; "Nanosecond";
     (* Errors *)
     "Bsf_not_found"; "Decoder_not_found"; "Demuxer_not_found";
     "Encoder_not_found"; "Eof"; "Exit"; "Filter_not_found"; "Invalid_data";
     "Muxer_not_found"; "Option_not_found"; "Patch_welcome"; "Protocol_not_found";
     "Stream_not_found"; "Bug"; "Eagain"; "Unknown"; "Experimental"; "Failure"];

  close_out pvv_oc;

  (* translate_enums parameters : *)
  (* in_name out_name title (start_pat, pat, end_pat, enum_prefix, c_type_name, c_fun_radix, ml_type_name) *)
  translate_enums "/libavcodec/avcodec.h" "codec_id" "Audio, video and subtitle codec ids" [
    "[ \t]*AV_CODEC_ID_NONE", "[ \t]*AV_CODEC_ID_\\([A-Z0-9_]+\\)", "[ \t]*AV_CODEC_ID_FIRST_AUDIO", "AV_CODEC_ID_", "enum AVCodecID", "VideoCodecID", "video";
    "", "[ \t]*AV_CODEC_ID_\\([A-Z0-9_]+\\)", "[ \t]*AV_CODEC_ID_FIRST_SUBTITLE", "AV_CODEC_ID_", "enum AVCodecID", "AudioCodecID", "audio";
    "", "[ \t]*AV_CODEC_ID_\\([A-Z0-9_]+\\)", "[ \t]*AV_CODEC_ID_FIRST_UNKNOWN", "AV_CODEC_ID_", "enum AVCodecID", "SubtitleCodecID", "subtitle"];

  translate_enums "/libavutil/pixfmt.h" "pixel_format" "Pixels formats" [
    "enum AVPixelFormat", "[ \t]*AV_PIX_FMT_\\([A-Z0-9_]+\\)", "[ \t]*AV_PIX_FMT_DRM_PRIME", "AV_PIX_FMT_", "enum AVPixelFormat", "PixelFormat", "t"];

  translate_enums "/libavutil/channel_layout.h" "channel_layout" "Channel layout formats" [
    "", "#define AV_CH_LAYOUT_\\([A-Z0-9_]+\\)", "", "AV_CH_LAYOUT_", "uint64_t", "ChannelLayout", "t"];

  translate_enums "/libavutil/samplefmt.h" "sample_format" "Audio sample formats" [
    "enum AVSampleFormat", "[ \t]*AV_SAMPLE_FMT_\\([A-Z0-9_]+\\)", "[ \t]*AV_SAMPLE_FMT_NB", "AV_SAMPLE_FMT_", "enum AVSampleFormat", "SampleFormat", "t"];

  translate_enums "/libswresample/swresample.h" "swresample_options" "Dithering algorithms, Resampling Engines and Resampling Filter Types options" [
    "[ \t]*SWR_DITHER_NONE", "[ \t]*SWR_\\([A-Z0-9_]+\\)", "[ \t]*SWR_DITHER_NS", "SWR_", "enum SwrDitherType", "DitherType", "dither_type";
    "enum SwrEngine", "[ \t]*SWR_\\([A-Z0-9_]+\\)", "[ \t]*SWR_ENGINE_NB", "SWR_", "enum SwrEngine", "Engine", "engine";
    "enum SwrFilterType", "[ \t]*SWR_\\([A-Z0-9_]+\\)", "\\};", "SWR_", "enum SwrFilterType", "FilterType", "filter_type"];
