open FFmpeg
open Avutil

let fill_yuv_image width height pixel_format frame nb_img write =
  print_string "Wrap                     : ";

  for frame_index = 0 to nb_img do

    let planes = Video.get_frame_planes frame in

    (* Y *)
    let plane_y = planes.(0) in
    let linesize_y = Video.frame_plane_get_linesize plane_y in

    for y = 0 to height - 1 do
      let off = y * linesize_y in
      for x = 0 to width - 1 do
        Video.frame_plane_set plane_y (x + off) (x + y + frame_index * 3);
      done;
    done;

    (* Cb and Cr *)
    let plane_cb = planes.(1) in
    let linesize_cb = Video.frame_plane_get_linesize plane_cb in
    let plane_cr = planes.(2) in

    for y = 0 to (height / 2) - 1 do
      let off = y * linesize_cb in
      for x = 0 to width / 2 - 1 do
        Video.frame_plane_set plane_cb (x + off) (128 + y + frame_index * 2);
        Video.frame_plane_set plane_cr (x + off) (64 + x + frame_index * 5);
      done;
    done;

    write frame;
  done
