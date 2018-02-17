open FFmpeg
open Avutil

let fill_yuv_image width height frame_index planes =
  (* Y *)
  let data_y, linesize_y = planes.(0) in

  for y = 0 to height - 1 do
    let off = y * linesize_y in
    for x = 0 to width - 1 do
      data_y.{x + off} <- x + y + frame_index * 3;
    done;
  done;

  (* Cb and Cr *)
  let data_cb, linesize_cb = planes.(1) in
  let data_cr, _ = planes.(2) in

  for y = 0 to (height / 2) - 1 do
    let off = y * linesize_cb in
    for x = 0 to width / 2 - 1 do
      data_cb.{x + off} <- 128 + y + frame_index * 2;
      data_cr.{x + off} <- 64 + x + frame_index * 5;
    done;
  done


let () =
  if Array.length Sys.argv < 3 then (
    Printf.eprintf "Usage: %s <output file> <codec name>\n" Sys.argv.(0);
    exit 1);

  let width = 352 in let height = 288 in let pixel_format = `YUV420P in
  let codec_name = Sys.argv.(2) in

  let dst = Av.open_output Sys.argv.(1) in

  let ovs = Av.new_video_stream ~codec_name ~width ~height ~pixel_format dst in

  let frame = Video.create_frame width height pixel_format in

  for i = 0 to 240 do
    Video.frame_visit ~make_writable:true (fill_yuv_image width height i) frame
    |> Av.write ovs;
  done;

  Av.close dst;

  Gc.full_major (); Gc.full_major ()
