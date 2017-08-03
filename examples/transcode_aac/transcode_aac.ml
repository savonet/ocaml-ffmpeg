open FFmpeg
open Avutil
open Avcodec
module CL = Avutil.Channel_layout
module Resampler = Swresample.Make (Swresample.Frame) (Swresample.Frame)

let () =
  if Array.length Sys.argv < 3 then (
    Printf.eprintf "Usage: %s <input_file> <output_file>.mp4\n" Sys.argv.(0);
    exit 1
  );

  let src = Av.open_input Sys.argv.(1) in

  let src_af = Av.get_audio_format src in
  
  let rsp = Resampler.from_audio_format_to_codec src_af
      Audio.AC_AAC CL.CL_stereo src_af.sample_rate
  in
  let dst = Av.open_output Sys.argv.(2) in
  
  let sid = Av.new_audio_stream dst Audio.AC_AAC CL.CL_stereo src_af.sample_rate
  in
  Av.iter_audio src
    (fun frame -> Resampler.convert rsp frame |> Av.write_audio_frame dst sid);

  Av.close_input src;
  Av.close_output dst;
  Gc.full_major ()
    
