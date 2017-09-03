open FFmpeg
module Resampler = Swresample.Make (Swresample.Frame) (Swresample.Frame)

let () =
  if Array.length Sys.argv < 3 then (
    Printf.eprintf "Usage: %s <input_file> <output_file>.mp4\n" Sys.argv.(0);
    exit 1);

  let src = Av.open_input Sys.argv.(1) in
  
  let dst = Av.open_output Sys.argv.(2) in
  
  let sid = Av.new_audio_stream ~codec_id:Avcodec.Audio.AC_AAC
      ~codec_parameters:(Av.get_input_audio_codec_parameters src) dst in

  let rsp = Resampler.from_input_to_output src dst in
  
  src |> Av.iter_audio (fun frame ->
      Resampler.convert rsp frame |> Av.write_audio_frame dst sid);

  Av.close_input src;
  Av.close_output dst;

  Gc.full_major ()
