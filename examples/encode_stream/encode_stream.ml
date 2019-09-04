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
  let out_sample_format = Audio.find_best_sample_format codec_id `Dbl in

  let rsp = Resampler.create `Mono sample_rate
      `Stereo ~out_sample_format sample_rate in

  let c = (2. *. pi *. 440.) /. (float_of_int sample_rate) in

  let filename = Sys.argv.(1) in
  let format =
    match Av.Format.guess_output_format ~filename () with
      | None -> failwith "No format for filename!"
      | Some f -> f
  in
  let fd = Unix.(openfile filename [O_WRONLY; O_CREAT; O_TRUNC] 0o644) in
  let write = Unix.write fd in
  let seek = Unix.lseek fd in
  let output = Av.open_output_stream format ~seek write in
  let stream = Av.new_audio_stream ~codec_id output in 

  for i = 0 to 2000 do
    Array.init frame_size (fun t -> sin(float_of_int(t + (i * frame_size)) *. c))
    |> Resampler.convert rsp
    |> Av.write_frame stream
  done;

  Av.close output;
  Unix.close fd;

  Gc.full_major (); Gc.full_major ()
