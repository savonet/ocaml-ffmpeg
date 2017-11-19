
external polymorphic_variant_string_to_c_value : string -> int64 = "polymorphic_variant_string_to_c_value"


let rec id_to_pv_value id values =
  let id = String.lowercase id in
  let id = if id.[0] >= '0' && id.[0] <= '9' then "_" ^ id else id in
  let value = polymorphic_variant_string_to_c_value id in

  if List.mem value values then
    id_to_pv_value(id ^ "_") values
  else (id, value)


let translate_enum_lines ic c_type_name ml_type_name prefix re end_re print_c print_ml =
  let rec loop values =
    try
      match input_line ic with
      | line when Str.string_match end_re line 0 -> values
      | line when Str.string_match re line 0 ->
        let id = Str.matched_group 1 line in
        let pv, value = id_to_pv_value id values in

        print_c("  {(" ^ Int64.to_string value ^ "), " ^ prefix ^ id ^ "},");
        print_ml("  | `" ^ pv);
        loop (value::values)
      | _ -> loop values
    with End_of_file -> values
  in
  print_c("static const long " ^ c_type_name ^ "[][2] = {");
  print_ml("type " ^ ml_type_name ^ " = [");
  let values = loop [] in
  print_c ("};\n#define " ^ c_type_name ^ "_LEN " ^ string_of_int(List.length values) ^ "\n");
  print_ml "]\n"


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


let () =
  match get_path"/libavcodec/avcodec.h" with
  | None -> ()
  | Some path -> (
      let ic = open_in path in

      let c_oc = open_out "codec_id.h" in
      let ml_oc = open_out "codec_id.ml" in
      let mli_oc = open_out "codec_id.mli" in

      let print_c line = output_string c_oc (line ^ "\n") in
      let print_ml line =
        output_string ml_oc (line ^ "\n");
        output_string mli_oc (line ^ "\n");
      in


      let codec_id_re = Str.regexp "[ \t]*AV_CODEC_ID_\\([A-Z0-9_]+\\)" in
      let end_re = Str.regexp "[ \t]*AV_CODEC_ID_NONE" in

      translate_enum_lines ic "" "" "" codec_id_re end_re (fun _->()) (fun _->());


      let end_re = Str.regexp "[ \t]*AV_CODEC_ID_FIRST_AUDIO" in

      translate_enum_lines ic "VIDEO_CODEC_IDS" "video" "AV_CODEC_ID_"
        codec_id_re end_re print_c print_ml;


      let end_re = Str.regexp "[ \t]*AV_CODEC_ID_FIRST_SUBTITLE" in

      translate_enum_lines ic "AUDIO_CODEC_IDS" "audio" "AV_CODEC_ID_"
        codec_id_re end_re print_c print_ml;


      let end_re = Str.regexp "[ \t]*AV_CODEC_ID_FIRST_UNKNOWN" in

      translate_enum_lines ic "SUBTITLE_CODEC_IDS" "subtitle" "AV_CODEC_ID_"
        codec_id_re end_re print_c print_ml;

      close_in ic;
      close_out c_oc;
      close_out ml_oc;
      close_out mli_oc;
    )
