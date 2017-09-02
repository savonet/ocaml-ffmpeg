open FFmpeg
open Avutil
open Printf
    
module CL = Avutil.Channel_layout
module Resampler = Swresample.Make (Swresample.Frame) (Swresample.Frame)

let () =
  if Array.length Sys.argv < 2 then (
    printf "input devices :";
    Avdevice.get_input_audio_devices() |> List.iter(fun d -> printf" %s"(Av.Input_format.get_name d));
    printf "\noutput devices :";
    Avdevice.get_output_audio_devices() |> List.iter(fun d -> printf" %s"(Av.Output_format.get_name d));
    
    printf "\nusage: %s input_name [output_device]\n" Sys.argv.(0);
    exit 1);

  let input_name = Sys.argv.(1) in
  
  let src = try Avdevice.get_input_audio_devices()
                   |> List.find(fun d -> Av.Input_format.get_name d = input_name)
                   |> Av.open_input_format
    with Not_found -> Av.open_input input_name in

  let output_name = if Array.length Sys.argv < 3 then "default" else Sys.argv.(2) in

  let out_fmts = Avdevice.get_output_audio_devices() in

  let out_fmt = try List.find(fun d -> Av.Output_format.get_name d = output_name) out_fmts
    with Not_found -> List.hd out_fmts in

  let rsp = Resampler.from_audio_format_to_codec (Av.get_audio_format src)
       (Av.Output_format.get_audio_codec out_fmt) CL.CL_stereo 44100 in

  let dst = Av.open_output_format out_fmt in

  src |> Av.iter_audio (fun frame ->
      Resampler.convert rsp frame |> Av.write_audio_frame dst 0);

  Av.close_input src;
  Av.close_output dst;

  Gc.full_major ()
