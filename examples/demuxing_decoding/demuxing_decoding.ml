open FFmpeg
open Avutil
open Av
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

  let input_filename = Sys.argv.(1) in
  let video_output_filename = Sys.argv.(2) in
  let audio_output_filename = Sys.argv.(3) in

  let input_file = Input.open_url input_filename in

  let rbs = FrameToS32Bytes.from_input input_file Channel_layout.CL_stereo 44100 in

  Input.get_metadata input_file |> List.iter(fun(k, v) -> print_endline(k^" : "^v));

  let video_output_file = open_out_bin video_output_filename in
  let audio_output_file = open_out_bin audio_output_filename in

  let rec decode() =
    match Input.read input_file with
    | Input.Audio (idx, af) ->
      FrameToS32Bytes.convert rbs af |> output_bytes audio_output_file;
      decode()
    | Input.Video (idx, vf) ->
      (*        Swscale.scale_frame vf |> output_video video_output_file;*)
      decode()
    | Input.Subtitle (idx, sf) -> let _, _, lines = Subtitle_frame.to_lines sf in
      lines |> List.iter print_endline;
      decode()
    | Input.End_of_file -> ()
    | exception Avutil.Failure msg -> prerr_endline msg
  in
  decode();

  Input.close input_file;

  close_out video_output_file;
  close_out audio_output_file;

  Printf.printf "Demuxing succeeded.\n";
  Printf.printf "Play the output audio file with the command:\nffplay -f %s -ac 2 -ar 44100 %s\n"
    (Sample_format.get_name Sample_format.SF_S32 ^ if Sys.big_endian then "be" else "le")
    audio_output_filename;

  Gc.full_major ()
