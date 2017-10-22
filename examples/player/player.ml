open FFmpeg

let () =
  if Array.length Sys.argv < 2 then (
    Printf.printf"usage: %s input_file\n" Sys.argv.(0);
    exit 0);

  let src = Av.open_input Sys.argv.(1) in

  let audio, close_audio = try
      let _, input_stream, _ = Av.find_best_audio_stream src in

      let dst = Avdevice.get_output_audio_devices() |> List.hd
                |> Av.open_output_format in

      Av.select input_stream;

      ((fun _ frame -> Av.write_audio dst frame), (fun () -> Av.close dst))
    with Avutil.Failure msg -> ((fun _ _ -> ()), (fun() -> ())) in

  let video, close_video = try
      let _, input_stream, _ = Av.find_best_video_stream src in

      let dst = Avdevice.find_output_video_device "xv"
                |> Av.open_output_format in

      Av.select input_stream;

      ((fun _ frame -> Av.write_video dst frame), (fun () -> Av.close dst))
    with Avutil.Failure msg -> ((fun _ _ -> ()), (fun() -> ())) in

  Av.iter_input ~audio ~video src;

  Av.close src;
  close_audio();
  close_video();

  Gc.full_major (); Gc.full_major ();
