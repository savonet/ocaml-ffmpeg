open FFmpeg
open Avutil
    
module FrameToS32Bytes = Swresample.Make (Swresample.Frame) (Swresample.S32Bytes)

let () =
  if Array.length Sys.argv < 3 then (
    Printf.eprintf "      usage: %s input_file audio_output_file
      API example program to show how to read audio frames from an input file.
      This program reads best audio frames from a file, decodes them, and writes decoded
      audio frames to a rawaudio file named audio_output_file.\n" Sys.argv.(0);
    exit 1
  );

  let audio_output_filename = Sys.argv.(2) ^ ".raw" in
  let audio_output_file = open_out_bin audio_output_filename in

  let src = Av.open_input Sys.argv.(1) in

  let rsp = FrameToS32Bytes.from_audio_format (Av.get_audio_format src)
      Channel_layout.CL_stereo 44100
  in
  Av.iter_audio (fun af ->
      FrameToS32Bytes.convert rsp af |> output_bytes audio_output_file) src;

  Av.close_input src;
  close_out audio_output_file;

  Printf.printf "Play the output audio file with the command:\nffplay -f %s -ac 2 -ar 44100 %s\n"
    (Sample_format.get_name Sample_format.SF_S32 ^ if Sys.big_endian then "be" else "le")
    audio_output_filename;

  Gc.full_major ()
