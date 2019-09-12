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

  let codec = Audio.find_encoder Sys.argv.(2) in
  let out_sample_format = Audio.find_best_sample_format codec `Dbl in

  let out_sample_rate =
    if Sys.argv.(2) = "flac" then
      22050
    else
      44100
  in

  let rsp = Resampler.create `Mono sample_rate
      `Stereo ~out_sample_format out_sample_rate in

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

  let output_opt = Hashtbl.create 2 in
  Hashtbl.add output_opt "packetsize" (`Int 4096);
  Hashtbl.add output_opt "foo" (`String "bla");

  let output = Av.open_output_stream ~opts:output_opt ~seek write format in

  let opts = Av.mk_audio_opts ~channels:2 ~sample_format:out_sample_format ~sample_rate:out_sample_rate () in
  Hashtbl.add opts "lpc_type" (`String "none");
  Hashtbl.add opts "foo" (`String "bla");

  let stream = Av.new_audio_stream ~codec ~opts output in 

  assert(Hashtbl.mem opts "foo");
  if Sys.argv.(2) = "flac" then
    assert(not (Hashtbl.mem opts "lpc_type"))
  else
    assert(Hashtbl.mem opts "lpc_type");

  assert(Hashtbl.mem output_opt "foo");
  assert(not (Hashtbl.mem output_opt "packetsize"));

  for i = 0 to 2000 do
    Array.init frame_size (fun t -> sin(float_of_int(t + (i * frame_size)) *. c))
    |> Resampler.convert rsp
    |> Av.write_frame stream
  done;

  Av.close output;
  Unix.close fd;

  Gc.full_major (); Gc.full_major ()
