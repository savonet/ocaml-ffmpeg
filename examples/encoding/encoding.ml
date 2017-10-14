open FFmpeg
open Avutil
module Resampler = Swresample.Make (Swresample.FloatArray) (Swresample.Frame)

let fill_image_y width height frame_index frame =
  (* Y *)
  for y = 0 to height - 1 do
    for x = 0 to width - 1 do
      Video.frame_set frame 0 x y (x + y + frame_index * 3)
    done;
  done

let fill_image_on width height frame_index frame =
  fill_image_y width height frame_index frame;
  (* Cb and Cr *)
  for y = 0 to (height / 2) - 1 do
    for x = 0 to (width / 2) - 1 do
      Video.frame_set frame 1 x y (128 + y + frame_index * 2);
      Video.frame_set frame 2 x y (64 + x + frame_index * 5);
    done;
  done;
  frame

let fill_image_off width height frame_index frame =
  fill_image_y width height frame_index frame;
  (* Cb and Cr *)
  for y = 0 to (height / 2) - 1 do
    for x = 0 to (width / 2) - 1 do
      Video.frame_set frame 1 x y 128;
      Video.frame_set frame 2 x y 128;
    done;
  done;
  frame

let () =
  if Array.length Sys.argv < 5 then (
    Printf.printf"Usage: %s <output file> <audio codec> <video codec> <subtitle codec>\n" Sys.argv.(0);
    exit 1);

  let dst = Av.open_output Sys.argv.(1) in

  Av.set_output_metadata dst ["Title", "On Off"];

  let pi = 4.0 *. atan 1.0 in let sample_rate = 44100 in

  let oas = Av.new_audio_stream ~codec_name:Sys.argv.(2)
      ~channel_layout:Channel_layout.CL_stereo ~sample_rate dst in

  Av.set_metadata oas ["Media", "Audio"];

  let rsp = Resampler.to_codec Channel_layout.CL_mono sample_rate
      (Av.get_codec oas) in

  let c = (2. *. pi *. 440.) /. (float_of_int sample_rate) in

  let audio = Array.init sample_rate
      (fun t -> if t < sample_rate / 2 then sin(float_of_int t *. c) else 0.) in

  let width = 352 in let height = 288 in let pixel_format = Pixel_format.YUV420p in
  let frame_rate = 25 in

  let ovs = Av.new_video_stream ~codec_name:Sys.argv.(3)
      ~width ~height ~pixel_format ~frame_rate dst in

  Av.set_metadata ovs ["Media", "Video"];

  let frame = Video.create_frame width height pixel_format in

  let video_on_off = [|fill_image_on; fill_image_off|] in
(*
  let oss = Av.new_subtitle_stream ~codec_name:Sys.argv.(4) dst in
  Av.set_metadata oas ["Media", "Subtitle"];
*)
  let duration = 10 in
  
  for i = 0 to duration - 1 do
    audio |> Resampler.convert rsp |> Av.write oas;
    (*
    let s = float_of_int i in
    Subtitle_frame.from_lines s (s +. 0.5) [string_of_int i]
    |> Av.write oss;
*)
  done;

  for i = 0 to frame_rate * duration - 1 do
    let b = (i mod frame_rate) / 13 in
    video_on_off.(b) width height i frame |> Av.write ovs;
  done;

  Av.close_output dst;
  Gc.full_major ()