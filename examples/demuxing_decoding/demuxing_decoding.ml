open FFmpeg
open Avutil

module FrameToS32Bytes = Swresample.Make (Swresample.Frame) (Swresample.S32Bytes)

let () =
  if Array.length Sys.argv < 4 then (
    Printf.eprintf"      usage: %s input_file video_output_file audio_output_file
      API example program to show how to read frames from an input file. 
      This program reads frames from a file, decodes them, and writes decoded
      video frames to a rawvideo file named video_output_file, and decoded
      audio frames to a rawaudio file named audio_output_file.\n" Sys.argv.(0);
    exit 1
  );

  let src_filename = Sys.argv.(1) in
  let video_dst_filename = Sys.argv.(2) in
  let audio_dst_filename = Sys.argv.(3) in

  let src_file = Av.open_input src_filename in

  let rbs = FrameToS32Bytes.of_in_audio_format (Av.get_audio_format src_file)
      Channel_layout.CL_stereo 44100
  in

  Av.get_metadata src_file |> List.iter(fun(k, v) -> print_endline(k^" : "^v));

  let video_out = open_out_bin video_dst_filename in
  let audio_out = open_out_bin audio_dst_filename in

  let rec decode() =
    match Av.read src_file with
    | Av.Audio (idx, af) ->
      FrameToS32Bytes.convert rbs af |> output_bytes audio_out;
      decode()
    | Av.Video (idx, vf) ->
      (*        Swscale.scale_frame vf |> output_string video_out;*)
      decode()
    | Av.Subtitle (idx, sf) -> Av.subtitle_to_string sf |> print_endline;
      decode()
    | Av.End_of_file -> ()
  in
  decode();

  close_out video_out;
  close_out audio_out;

  Printf.printf "Demuxing succeeded.\n";
  Printf.printf "Play the output audio file with the command:\nffplay -f %s -ac 2 -ar 44100 %s\n"
    (Sample_format.get_name Sample_format.SF_S32 ^ if Sys.big_endian then "be" else "le")
    audio_dst_filename
