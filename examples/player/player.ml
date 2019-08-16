open FFmpeg

let () =
  if Array.length Sys.argv < 2 then (
    Printf.printf"usage: %s input_file\n" Sys.argv.(0);
    exit 0);

  Avutil.Log.set_level `Debug;
  Avutil.Log.set_callback print_string;
  
  let src = Av.open_input Sys.argv.(1) in

  let audio, close_audio = try
      let _, input_stream, _ = Av.find_best_audio_stream src in

      let dst = Avdevice.open_default_audio_output() in

      Av.select input_stream;

      ((fun _ frame -> Av.write_audio_frame dst frame), (fun() -> Av.close dst))
    with Avutil.Error _ -> ((fun _ _ -> ()), (fun() -> ())) in

  let video, close_video = try
      let _, input_stream, _ = Av.find_best_video_stream src in

      let dst = Avdevice.open_video_output "xv" in

      Av.select input_stream;

      ((fun _ frame -> Av.write_video_frame dst frame), (fun() -> Av.close dst))
    with Avutil.Error _ -> ((fun _ _ -> ()), (fun() -> ())) in

  Av.iter_input_frame ~audio ~video src;

  Av.close src;
  close_audio();
  close_video();

  Gc.full_major (); Gc.full_major ();
