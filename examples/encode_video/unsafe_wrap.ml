open FFmpeg
open Avutil

let fill_image width height pixel_format frame nb_img write =

  let planes = Video.get_frame_planes frame in

  let ba_y, linesize_y, _ = planes.(0) in
  let ba_cb, linesize_cb, _ = planes.(1) in
  let ba_cr, _, _ = planes.(2) in

  for frame_index = 0 to nb_img do
    (* Y *)
    for y = 0 to height - 1 do
      let off = y * linesize_y in
      for x = 0 to width - 1 do
        Bigarray.Array1.unsafe_set ba_y (x + off) (x + y + frame_index * 3);
      done;
    done;

    (* Cb and Cr *)
    for y = 0 to (height / 2) - 1 do
      let off = y * linesize_cb in
      for x = 0 to width / 2 - 1 do
        Bigarray.Array1.unsafe_set ba_cb (x + off) (128 + y + frame_index * 2);
        Bigarray.Array1.unsafe_set ba_cr (x + off) (64 + x + frame_index * 5);
      done;
    done;

    write frame;
  done;
  "Unsafe_wrap.fill_image"

