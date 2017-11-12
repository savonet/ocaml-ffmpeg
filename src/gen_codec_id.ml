let translate_enum_lines ic c_type_name ml_type_name prefix re end_re print_c print_ml =
let rec loop() =
match input_line ic with
| line when Str.string_match end_re line 0 -> ()
| line when Str.string_match re line 0 ->
(Str.matched_string line) ^ ","
|> print_c;

Str.matched_group 1 line
(*
|> String.lowercase_ascii
|> (fun s -> "  | `" ^ if s.[0] >= '0' && s.[0] <= '9' then "_" ^ s else s)
*)
|> (fun s -> "    | " ^ prefix ^ "_" ^ s)
|> print_ml;

loop()
| _ -> loop()
| exception End_of_file -> ()
in
print_c("static const enum " ^ c_type_name ^ "[] = {");
print_ml("module " ^ ml_type_name ^ " = struct\n  type id =");
loop();
print_c "};";
print_ml "end"


let () =

let ic = open_in "/usr/include/i386-linux-gnu/libavcodec/avcodec.h" in

let c_oc = open_out "codec_id.h" in
let ml_oc = open_out "codec.ml" in

let print_c line = output_string c_oc (line ^ "\n") in
let print_ml line = output_string ml_oc (line ^ "\n") in


let codec_id_re = Str.regexp "[ \t]*AV_CODEC_ID_\\([A-Z0-9_]+\\)" in
let before_end_re = Str.regexp "[ \t]*AV_CODEC_ID_NONE" in

translate_enum_lines ic "" "" "" codec_id_re before_end_re (fun _->()) (fun _->());


let video_end_re = Str.regexp "[ \t]*AV_CODEC_ID_FIRST_AUDIO" in

translate_enum_lines ic "AVCodecID VIDEO_CODEC_IDS" "Video" "VC"
codec_id_re video_end_re print_c print_ml;


let audio_end_re = Str.regexp "[ \t]*AV_CODEC_ID_FIRST_SUBTITLE" in

translate_enum_lines ic "AVCodecID AUDIO_CODEC_IDS" "Audio" "AC"
codec_id_re audio_end_re print_c print_ml;


let subtitle_end_re = Str.regexp "[ \t]*AV_CODEC_ID_FIRST_UNKNOWN" in

translate_enum_lines ic "AVCodecID SUBTITLE_CODEC_IDS" "Subtitle" "SC"
codec_id_re subtitle_end_re print_c print_ml;

close_in ic;
close_out c_oc;
close_out ml_oc;
