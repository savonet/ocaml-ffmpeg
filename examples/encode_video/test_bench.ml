open FFmpeg
open Avutil

let () =
  let fillers = [Set.fill_image; Planar_set.fill_image; Bigarray_copy.fill_image; Bytes_copy.fill_image; Bigarray_scale.fill_image; Bytes_scale.fill_image; Wrap.fill_image] in

  let getters = [Set.get_image; Planar_set.get_image; Bigarray_copy.get_image; Bytes_copy.get_image; Wrap.get_image] in

  let width = 1024 in let height = 768 in let pixel_format = `YUV420P in
  let frame = Video.create_frame width height pixel_format in

  (* let dst = Av.open_output ("out.ogg") in
   * let ovs = Av.new_video_stream ~codec_name:"libtheora" ~width ~height ~pixel_format dst in *)

  let run funs =
  funs |> List.map(fun fill_image ->
      let start = Unix.gettimeofday() in
      let fun_name = fill_image width height pixel_format frame 250
          (fun _ -> ());
        (* (Av.write ovs); *)
      in
      (fun_name, Unix.gettimeofday() -. start)
    )
  |> List.sort(fun (_, d1) (_, d2) -> compare d1 d2)
  |> List.iter(fun(fn, d) -> Printf.printf "% -40s : %f seconds\n" fn d);
  in
  run getters;
  print_newline();
  run fillers;
  (* Av.close dst; *)
