open FFmpeg
module Resampler = Swresample.Make (Swresample.FloatArray) (Swresample.Frame)

let () =
  if Array.length Sys.argv < 3 then (
    Printf.eprintf "Usage: %s <output file> <codec name>\n" Sys.argv.(0);
    exit 1);

  let pi = 4.0 *. atan 1.0 in let sample_rate = 44100 in let frame_size = 64 in

  let codec_id = Avcodec.Audio.find_id Sys.argv.(2) in

  let ctx = Avcodec.Audio.create_encoder_context codec_id in
  let out_sample_format = Avcodec.Audio.find_best_sample_format codec_id in
  
  let rsp = Resampler.create `MONO sample_rate
      `STEREO ~out_sample_format sample_rate in

  let c = (2. *. pi *. 440.) /. (float_of_int sample_rate) in

  let out_file = open_out_bin Sys.argv.(1) in

  for i = 0 to 200 do
    Array.init frame_size (fun t -> sin(float_of_int(t + (i * frame_size)) *. c))
    |> Resampler.convert rsp
    |> Avcodec.Audio.encode ctx
    |> output_bytes out_file;
  done;

  close_out out_file;

  Gc.full_major (); Gc.full_major ()
