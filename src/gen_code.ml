
external polymorphic_variant_string_to_c_value : string -> int64 = "polymorphic_variant_string_to_c_value"


let print_define_polymorphic_variant_value oc pv =
  let value = Int64.to_string(polymorphic_variant_string_to_c_value pv) in
  output_string oc ("#define PVV_" ^ pv ^ " (" ^ value ^ ")\n")


let rec find_line ic line_re = try
    if Str.string_match line_re (input_line ic) 0 then true
    else find_line ic line_re
  with End_of_file -> false


let get_path filename =
  Sys.argv |> Array.fold_left(fun path param ->
      if path = None && "-I" = String.sub param 0 2 then (
        let p = (String.sub param 2 (String.length param - 2)) ^ filename in
        if Sys.file_exists p then Some p
        else (
          print_endline("File " ^ filename ^ " not found");
          None)
      )
      else path
    ) None


let rec id_to_pv_value id values =
  (* let id = String.lowercase id in *)
  let id = if id.[0] >= '0' && id.[0] <= '9' then "_" ^ id else id in
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

    print_c[c_type_name;" ";c_fun_radix;"_val(value v){\nint i;\nfor(i=0;i<";tab_len;";i++){\nif(v==";tab_name;"[i][0])return ";tab_name;"[i][1];\n}\nreturn 0;\n}"];

    print_c["value Val_";c_fun_radix;"(";c_type_name;" t){\nint i;\nfor(i=0;i<";tab_len;"; i++){\nif(t==";tab_name;"[i][1])return ";tab_name;"[i][0];\n}\nreturn ";tab_name;"[0][0];\n}"];

    print_ml["]\n"];
  )


let translate_enums in_name out_name enums_labels =

  match get_path in_name with
  | None -> Printf.eprintf"WARNING : header file %s not found\n" in_name
  | Some path ->
    let ic = open_in path in

    let c_oc = open_out (out_name^".h") in
    let ml_oc = open_out (out_name^".ml") in
    let mli_oc = open_out (out_name^".mli") in

    List.iter(fun labels -> translate_enum_lines ic labels c_oc ml_oc mli_oc) enums_labels;

    close_in ic;
    close_out c_oc;
    close_out ml_oc;
    close_out mli_oc


let () =
  let pvv_oc = open_out "polymorphic_variant_values.h" in

  List.iter(print_define_polymorphic_variant_value pvv_oc)
    ["frame"; "audio"; "video"; "subtitle"; "end_of_stream"; "end_of_file";
     "error"; "second"; "millisecond"; "microsecond"; "nanosecond"];

  close_out pvv_oc;

  (* translate_enums parameters : *)
  (* in_name out_name (start_pat, pat, end_pat, enum_prefix, c_type_name, c_fun_radix, ml_type_name) *)
  translate_enums "/libavcodec/avcodec.h" "codec_id" [
    "[ \t]*AV_CODEC_ID_NONE", "[ \t]*AV_CODEC_ID_\\([A-Z0-9_]+\\)", "[ \t]*AV_CODEC_ID_FIRST_AUDIO", "AV_CODEC_ID_", "enum AVCodecID", "VideoCodecID", "video";
    "", "[ \t]*AV_CODEC_ID_\\([A-Z0-9_]+\\)", "[ \t]*AV_CODEC_ID_FIRST_SUBTITLE", "AV_CODEC_ID_", "enum AVCodecID", "AudioCodecID", "audio";
    "", "[ \t]*AV_CODEC_ID_\\([A-Z0-9_]+\\)", "[ \t]*AV_CODEC_ID_FIRST_UNKNOWN", "AV_CODEC_ID_", "enum AVCodecID", "SubtitleCodecID", "subtitle"];

  translate_enums "/libavutil/pixfmt.h" "pix_fmt" [
    "", "[ \t]*AV_PIX_FMT_\\([A-Z0-9_]+\\)", "[ \t]*AV_PIX_FMT_DRM_PRIME", "AV_PIX_FMT_", "enum AVPixelFormat", "PixelFormat", "t"];

  translate_enums "/libavutil/channel_layout.h" "ch_layout" [
    "", "#define AV_CH_LAYOUT_\\([A-Z0-9_]+\\)", "", "AV_CH_LAYOUT_", "uint64_t", "ChannelLayout", "t"];

  translate_enums "/libavutil/samplefmt.h" "sample_fmt" [
    "", "[ \t]*AV_SAMPLE_FMT_\\([A-Z0-9_]+\\)", "[ \t]*AV_SAMPLE_FMT_NB", "AV_SAMPLE_FMT_", "enum AVSampleFormat", "SampleFormat", "t"];
