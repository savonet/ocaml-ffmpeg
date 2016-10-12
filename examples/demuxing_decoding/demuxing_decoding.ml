open FFmpeg
open Avutil

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
  let srs = Av.create_resample src_file in

  Av.get_metadata src_file |> List.iter(fun (k, v) -> print_endline(k^" : "^v));
  
  let video_dst_file = open_out_bin video_dst_filename in
  let audio_dst_file = open_out_bin audio_dst_filename in

  let rec decode() = Av.(
      match read src_file with
      | Audio idx, af ->
        Swresample.to_string srs af |> output_string audio_dst_file;
        decode()
      | Video idx, vf ->
        video_to_string f |> output_string video_dst_file;
        decode()
      | Subtitle idx, sf ->
        subtitle_to_string f |> print_endline;
        decode()
      | exception Av.End_of_file -> ()
    )
  in
  decode();

  close_out video_dst_file;
  close_out audio_dst_file;

  Printf.printf "Demuxing succeeded.\n";
  Printf.printf "Play the output audio file with the command:\nffplay -f %s -ac %d -ar %d %s\n"
    Av.(get_audio_sample_format src_file |> Sample_format.get_name)
    Av.(get_audio_nb_channels src_file)
    Av.(get_audio_sample_rate src_file)
    audio_dst_filename
