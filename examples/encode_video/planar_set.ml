open FFmpeg
open Avutil

let fill_image width height pixel_format frame nb_img write =

  for frame_index = 0 to nb_img do
    let y_linesize = Video.frame_get_linesize frame 0 in
    for y = 0 to height - 1 do
      let off = y * y_linesize in
      for x = 0 to width - 1 do
        Video.frame_planar_set frame 0 (x + off) (x + y + frame_index * 3)
      done;
    done;

    let cb_cr_linesize = Video.frame_get_linesize frame 1 in
    for y = 0 to (height / 2) - 1 do
      let off = y * cb_cr_linesize in
      for x = 0 to (width / 2) - 1 do
        Video.frame_planar_set frame 1 (x + off) (128 + y + frame_index * 2);
        Video.frame_planar_set frame 2 (x + off) (64 + x + frame_index * 5);
      done;
    done;

    write frame;
  done;
  "Planar_set.fill_image"


let get_image width height pixel_format frame nb_img _ =

  let r = ref 0 in
  for frame_index = 0 to nb_img do
    let y_linesize = Video.frame_get_linesize frame 0 in
    for y = 0 to height - 1 do
      let off = y * y_linesize in
      for x = 0 to width - 1 do
        r := !r + Video.frame_planar_get frame 0 (x + off)
      done;
    done;
  done;
  "Planar_set.get_image "^(string_of_int !r)


