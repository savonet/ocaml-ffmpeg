open FFmpeg
open Avutil
module AudioConverter = Swresample.Make (Swresample.Frame) (Swresample.S32Bytes)
module VideoConverter = Swscale.Make (Swscale.Frame) (Swscale.BigArray)
(* module VideoConverter = Swscale.Make (Swscale.Frame) (Swscale.Frame) *)

let () =
  if Array.length Sys.argv < 4 then (
    Printf.eprintf"      usage: %s input_file video_output_file audio_output_file
      API example program to show how to read frames from an input file. 
      This program reads frames from a file, decodes them, and writes decoded
      video frames to a rawvideo file named video_output_file, and decoded
      audio frames to a rawaudio file named audio_output_file.\n" Sys.argv.(0);
    exit 1
  );

  Avutil.Log.set_level `Debug;
  Avutil.Log.set_callback print_string;
  
  let input_filename = Sys.argv.(1) in
  let video_output_filename = Sys.argv.(2) in
  let audio_output_filename = Sys.argv.(3) in

  let src = Av.open_input input_filename in
  Av.get_input_metadata src |> List.iter(fun(k, v) -> print_endline(k^" : "^v));

  let (audio_index, audio_stream, audio_codec) = Av.find_best_audio_stream src in

  let a_ctx = AudioConverter.from_codec audio_codec `Stereo 44100 in
  let audio_output_file = open_out_bin audio_output_filename in

  let (video_index, video_stream, video_codec) = Av.find_best_video_stream src in

  (* let v_ctx = VideoConverter.from_codec video_codec 800 600 `Yuv420p in *)
  let v_ctx = VideoConverter.create [] 352 288 `Yuv420p 800 600 `Yuv420p in
  let video_output_file = open_out_bin video_output_filename in

  let rec decode() =
    match Av.read_input_frame src with
    | `Audio (idx, af) ->
      if idx = audio_index then (
        AudioConverter.convert a_ctx af |> output_bytes audio_output_file);
      decode()
    | `Video (idx, vf) ->
      if idx = video_index then (
        VideoConverter.convert v_ctx vf |> ignore(*output_video video_output_file*));
      decode()
    | `Subtitle (idx, sf) ->
      let _, _, lines = Subtitle.frame_to_lines sf in
      lines |> List.iter print_endline;
      decode()
    | exception Error `Eof -> ()
    | exception Error err -> prerr_endline (Avutil.string_of_error err)
  in
  decode();

  Av.close src;

  close_out video_output_file;
  close_out audio_output_file;

  Printf.printf "Demuxing succeeded.\n";
  Printf.printf "Play the output audio file with the command:\nffplay -f %s -ac 2 -ar 44100 %s\n"
    (Sample_format.get_name `S32 ^ if Sys.big_endian then "be" else "le")
    audio_output_filename;

  Gc.full_major (); Gc.full_major ()
