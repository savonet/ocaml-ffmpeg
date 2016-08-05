open FFmpeg

let get_format_from_sample_fmt = function
  | Av.None -> ""
  | Av.Unsigned_8 | Av.Unsigned_8_planar -> "u8"
  | Av.Signed_16 | Av.Signed_16_planar -> "s16le"
  | Av.Signed_32 | Av.Signed_32_planar -> "s32le"
  | Av.Float_32 | Av.Float_32_planar -> "f32le"
  | Av.Float_64 | Av.Float_64_planar -> "f64le"


let () =
  if Array.length Sys.argv < 4 then (
    Printf.eprintf {|
      usage: %s input_file video_output_file audio_output_file
      API example program to show how to read frames from an input file. 
      This program reads frames from a file, decodes them, and writes decoded
      video frames to a rawvideo file named video_output_file, and decoded
      audio frames to a rawaudio file named audio_output_file.

|}    Sys.argv.(0);
    exit 1
  );

  let src_filename = Sys.argv.(1) in
  let video_dst_filename = Sys.argv.(2) in
  let audio_dst_filename = Sys.argv.(3) in

  let src_file = Av.open_input src_filename in
  let video_dst_file = open_out_bin video_dst_filename in
  let audio_dst_file = open_out_bin audio_dst_filename in

  let rec run() =
    match Av.(read_audio_frame src_file |> to_string) with
    | b -> output_string audio_dst_file b; run()
    | exception Av.End_of_file -> Printf.printf "End of %s\n" src_filename
  in
  run();

  Printf.printf "Demuxing succeeded.\n";
  Printf.printf "Play the output audio file with the command:\nffplay -f %s -ac %d -ar %d %s\n"
    (Av.get_audio_out_sample_format src_file |> get_format_from_sample_fmt)
    (Av.get_audio_out_nb_channels src_file)
    (Av.get_audio_out_sample_rate src_file)
    audio_dst_filename;
(*
let rec run() = Av.(
    match read_frame src_file with
    | AudioFrame f -> let b = to_float_planar_array f in run()
    | VideoFrame f -> let b = toBuffer f in run() 
    | SubtitleFrame f -> let b = to_string f in run()
    | exception Av.End_of_file -> ()
  )
in
run()
*)
  close_out video_dst_file;
  close_out audio_dst_file
