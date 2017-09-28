open FFmpeg
open Avutil
open Av
module Resampler = Swresample.Make (Swresample.FloatArray) (Swresample.Frame)

let fill_image_y width height frame_index frame =
  (* Y *)
  for y = 0 to height - 1 do
    for x = 0 to width - 1 do
      Video_frame.set frame 0 x y (x + y + frame_index * 3)
    done;
  done

let fill_image_on width height frame_index frame =
  fill_image_y width height frame_index frame;
  (* Cb and Cr *)
  for y = 0 to (height / 2) - 1 do
    for x = 0 to (width / 2) - 1 do
      Video_frame.set frame 1 x y (128 + y + frame_index * 2);
      Video_frame.set frame 2 x y (64 + x + frame_index * 5);
    done;
  done;
  frame

let fill_image_off width height frame_index frame =
  fill_image_y width height frame_index frame;
  (* Cb and Cr *)
  for y = 0 to (height / 2) - 1 do
    for x = 0 to (width / 2) - 1 do
      Video_frame.set frame 1 x y 128;
      Video_frame.set frame 2 x y 128;
    done;
  done;
  frame

let () =
  if Array.length Sys.argv < 5 then (
    Printf.printf"Usage: %s <output file> <audio codec> <video codec> <subtitle codec>\n" Sys.argv.(0);
    exit 1);

  let dst = Output.open_file Sys.argv.(1) in

  Output.set_metadata dst ["Title", "On Off"];

  let pi = 4.0 *. atan 1.0 in let sample_rate = 44100 in

  let asi = Output.new_audio_stream ~codec_name:Sys.argv.(2)
      ~channel_layout:Channel_layout.CL_stereo ~sample_rate dst in

  let rsp = Resampler.to_output Channel_layout.CL_mono sample_rate dst in

  let c = (2. *. pi *. 440.) /. (float_of_int sample_rate) in

  let audio = Array.init sample_rate
      (fun t -> if t < sample_rate / 2 then sin(float_of_int t *. c) else 0.) in

  let width = 352 in let height = 288 in let pixel_format = Pixel_format.YUV420p in
  let frame_rate = 25 in

  let vsi = Output.new_video_stream ~codec_name:Sys.argv.(3)
      ~width ~height ~pixel_format ~frame_rate dst in

  let frame = Video_frame.create width height pixel_format in

  let video_on_off = [|fill_image_on; fill_image_off|] in
(*
  let ssi = Output.new_subtitle_stream ~codec_name:Sys.argv.(4) dst in
*)
  for i = 0 to 29 do
    audio |> Resampler.convert rsp |> Output.write_audio_frame dst asi;
    (*
    let s = float_of_int i in
    Subtitle_frame.from_lines s (s +. 0.5) [string_of_int i]
    |> Output.write_subtitle_frame dst ssi;
*)
  done;

  for i = 0 to frame_rate * 30 - 1 do
    let b = (i mod frame_rate) / 13 in
    video_on_off.(b) width height i frame |> Output.write_video_frame dst vsi;
  done;

  Output.close dst;
  Gc.full_major ()
