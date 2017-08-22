open FFmpeg
open Avutil

module CL = Avutil.Channel_layout
module Resampler = Swresample.Make (Swresample.Frame) (Swresample.Frame)

let () =
  if Array.length Sys.argv < 2 then (
    Printf.eprintf "usage: %s input_file [device_name]\n" Sys.argv.(0);
    exit 1
  );

  let src = Av.open_input Sys.argv.(1) in

  let out_fmt = Avdevice.get_output_audio_devices() |> List.hd in

  let rsp = Resampler.from_audio_format_to_codec (Av.get_audio_format src)
       (Av.Output_format.get_audio_codec out_fmt) CL.CL_stereo 44100 in

  let dst = if Array.length Sys.argv < 3 then Av.open_output_format out_fmt
    else Av.open_output_format_name Sys.argv.(2) in

  src |> Av.iter_audio (fun frame ->
      Resampler.convert rsp frame |> Av.write_audio_frame dst 0);

  Av.close_input src;
  Av.close_output dst;

  Gc.full_major ()
