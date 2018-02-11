open FFmpeg
open Avutil

let () =
  let fillers = [Set.fill_yuv_image; Planar_set.fill_yuv_image; Bigarray_copy.fill_yuv_image; Bytes_copy.fill_yuv_image; Bigarray_scale.fill_yuv_image; Bytes_scale.fill_yuv_image; Wrap.fill_yuv_image; Unsafe_wrap.fill_yuv_image] in

  let width = 1024 in let height = 768 in let pixel_format = `YUV420P in
  let frame = Video.create_frame width height pixel_format in

  (* let dst = Av.open_output ("out.ogg") in
   * let ovs = Av.new_video_stream ~codec_name:"libtheora" ~width ~height ~pixel_format dst in *)

  List.iter(fun fill_yuv_image ->
      let start = Unix.gettimeofday() in
      fill_yuv_image width height pixel_format frame 250
        (fun _ -> ());
      (* (Av.write ovs); *)
      Printf.printf "%f seconds\n%!" (Unix.gettimeofday() -. start);
    )
    fillers;

  (* Av.close dst; *)
