open FFmpeg
module CL = Avutil.Channel_layout
module Resampler = Swresample.Make (Swresample.FloatArray) (Swresample.Frame)

let () =
  if Array.length Sys.argv < 3 then (
    Printf.eprintf "Usage: %s <output file> <codec name>\n" Sys.argv.(0);
    exit 1);

  let pi = 4.0 *. atan 1.0 in let sample_rate = 44100 in

  let codec_id = Avcodec.Audio.find_by_name Sys.argv.(2) in

  let oas = Av.open_output Sys.argv.(1) |> Av.new_audio_stream ~codec_id
              ~channel_layout:CL.CL_stereo ~sample_rate in

  let rsp = Resampler.to_codec CL.CL_mono sample_rate (Av.get_codec oas) in

  let c = (2. *. pi *. 440.) /. (float_of_int sample_rate) in

  for i = 0 to 200 do
    Array.init 1024 (fun t -> sin(float_of_int(t + (i * 1024)) *. c))
    |> Resampler.convert rsp |> Av.write oas;
  done;

  Av.get_output oas |> Av.close_output;

  Gc.full_major ()
