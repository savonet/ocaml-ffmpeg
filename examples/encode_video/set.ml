open FFmpeg
open Avutil

let fill_image width height pixel_format frame nb_img write =

  for frame_index = 0 to nb_img do
    (* Y *)
    for y = 0 to height - 1 do
      for x = 0 to width - 1 do
        Video.frame_set frame 0 x y (x + y + frame_index * 3)
      done;
    done;

    (* Cb and Cr *)
    for y = 0 to (height / 2) - 1 do
      for x = 0 to (width / 2) - 1 do
        Video.frame_set frame 1 x y (128 + y + frame_index * 2);
        Video.frame_set frame 2 x y (64 + x + frame_index * 5);
      done;
    done;

    write frame;
  done;
  "Set.fill_image"

let get_image width height pixel_format frame nb_img _ =

  let r = ref 0 in
  for frame_index = 0 to nb_img do
    for y = 0 to height - 1 do
      for x = 0 to width - 1 do
        r := !r + Video.frame_get frame 0 x y
      done;
    done;
  done;
  "Set.get_image "^(string_of_int !r)

