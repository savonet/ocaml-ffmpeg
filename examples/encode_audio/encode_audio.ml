open FFmpeg
open Avcodec
module Resampler = Swresample.Make (Swresample.FloatArray) (Swresample.Frame)

let (%>) f g x = g(f x)

let () =
  Printexc.record_backtrace true

let () =
  if Array.length Sys.argv < 3 then (
    Printf.eprintf "Usage: %s <output file> <codec name>\n" Sys.argv.(0);
    exit 1);

  Avutil.Log.set_level `Debug;
  Avutil.Log.set_callback print_string;
  
  let pi = 4.0 *. atan 1.0 in let sample_rate = 44100 in let frame_size = 512 in

  let codec_id = Audio.find_encoder_id Sys.argv.(2) in
  let encoder = Audio.create_encoder codec_id in
  let out_sample_format = Audio.find_best_sample_format codec_id `Dbl in

  let rsp = Resampler.create `Mono sample_rate
      `Stereo ~out_sample_format sample_rate in

  let c = (2. *. pi *. 440.) /. (float_of_int sample_rate) in

  let out_file = open_out_bin Sys.argv.(1) in

  for i = 0 to 2000 do
    Array.init frame_size (fun t -> sin(float_of_int(t + (i * frame_size)) *. c))
    |> Resampler.convert rsp
    |> Avcodec.encode encoder (Packet.to_bytes %> output_bytes out_file)
  done;

  Avcodec.flush_encoder encoder (Packet.to_bytes %> output_bytes out_file);

  close_out out_file;

  Gc.full_major (); Gc.full_major ()
