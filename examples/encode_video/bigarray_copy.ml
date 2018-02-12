open FFmpeg
open Avutil

let fill_image width height pixel_format frame nb_img write =

  let planes = Video.frame_to_bigarray_planes frame in

  for frame_index = 0 to nb_img do
    (* Y *)
    let data_y, linesize_y = planes.(0) in
    for y = 0 to height - 1 do
      let off = y * linesize_y in
      for x = 0 to width - 1 do
        data_y.{x + off} <- x + y + frame_index * 3;
      done;
    done;

    (* Cb and Cr *)
    let data_cb, _ = planes.(1) in
    let data_cr, linesize = planes.(2) in

    for y = 0 to (height / 2) - 1 do
      let off = y * linesize in
      for x = 0 to width / 2 - 1 do
        data_cb.{x + off} <- 128 + y + frame_index * 2;
        data_cr.{x + off} <- 64 + x + frame_index * 5;
      done;
    done;

    Video.bigarray_planes_to_frame planes frame;
    write frame;
  done;
  "Bigarray_copy.fill_image"

let get_image width height pixel_format frame nb_img _ =

  let r = ref 0 in

  for frame_index = 0 to nb_img do
    let planes = Video.frame_to_bigarray_planes frame in
    let data_y, linesize_y = planes.(0) in

    for y = 0 to height - 1 do
      let off = y * linesize_y in
      for x = 0 to width - 1 do
        r := !r + data_y.{x + off};
      done;
    done;
  done;
  "Bigarray_copy.get_image "^(string_of_int !r)
