open FFmpeg
open Avutil
module Resampler = Swresample.Make (Swresample.FloatArray) (Swresample.Frame)

let fill_image_y width height frame_index planes =
  (* Y *)
  let data, linesize = planes.(0) in
  for y = 0 to height - 1 do
    let off = y * linesize in
    for x = 0 to width - 1 do
      data.{x + off} <- x + y + frame_index * 3;
    done;
  done

let fill_image_on width height frame_index planes =
  fill_image_y width height frame_index planes;
  (* Cb and Cr *)
  let data_cb, _ = planes.(1) in
  let data_cr, linesize = planes.(2) in

  for y = 0 to (height / 2) - 1 do
    let off = y * linesize in
    for x = 0 to width / 2 - 1 do
      data_cb.{x + off} <- 128 + y + frame_index * 2;
      data_cr.{x + off} <- 64 + x + frame_index * 5;
    done;
  done

let fill_image_off width height frame_index planes =
  fill_image_y width height frame_index planes;
  (* Cb and Cr *)
  let data_cb, _ = planes.(1) in
  let data_cr, linesize = planes.(2) in

  for y = 0 to (height / 2) - 1 do
    let off = y * linesize in
    for x = 0 to width / 2 - 1 do
      data_cb.{x + off} <- 128;
      data_cr.{x + off} <- 128;
    done;
  done

let () =
  if Array.length Sys.argv < 5 then (
    Printf.printf"Usage: %s <output file> <audio codec> <video codec> <subtitle codec>\n" Sys.argv.(0);
    exit 1);

  Avutil.Log.set_level `Debug;
  Avutil.Log.set_callback print_string;
  
  let dst = Av.open_output Sys.argv.(1) in

  Av.set_output_metadata dst ["Title", "On Off"];

  let pi = 4.0 *. atan 1.0 in let sample_rate = 44100 in

  let codec = Avcodec.Audio.find_encoder Sys.argv.(2) in

  let opts = Av.mk_audio_opts ~channel_layout:`Stereo ~sample_rate () in

  let oas = Av.new_audio_stream ~codec ~opts dst in

  Av.set_metadata oas ["Media", "Audio"];

  let rsp = Resampler.to_codec `Mono sample_rate (Av.get_codec_params oas) in

  let c = (2. *. pi *. 440.) /. (float_of_int sample_rate) in

  let audio_on_off = [|Array.init 1804 (fun t -> sin(float_of_int t *. c));
                       Array.make 1804 0.|] in

  let width = 352 in let height = 288 in let pixel_format = `Yuv420p in
  let frame_rate = 25 in

  let codec = Avcodec.Video.find_encoder Sys.argv.(3) in
  let opts = Av.mk_video_opts ~size:(width,height)  ~pixel_format ~frame_rate () in

  let ovs = Av.new_video_stream ~codec ~opts dst in

  Av.set_metadata ovs ["Media", "Video"];

  let frame = Video.create_frame width height pixel_format in
  let video_on_off = [|fill_image_on; fill_image_off|] in
(*
  let oss = Av.new_subtitle_stream ~codec_name:Sys.argv.(4) dst in
  Av.set_metadata oas ["Media", "Subtitle"];
*)
  let duration = 10 in
(*  
  for i = 0 to duration - 1 do
    let s = float_of_int i in
    Subtitle_frame.from_lines s (s +. 0.5) [string_of_int i]
    |> Av.write oss;
  done;
*)
  for i = 0 to frame_rate * duration - 1 do
    let b = (i mod frame_rate) / 13 in

    audio_on_off.(b) |> Resampler.convert rsp |> Av.write_frame oas;

    Video.frame_visit ~make_writable:true (video_on_off.(b) width height i) frame
    |> Av.write_frame ovs;
  done;

  Av.close dst;

  Gc.full_major (); Gc.full_major ()
