open FFmpeg

let () =
  if Array.length Sys.argv < 2 then Printf.(Av.Format.(
      printf "\ninput devices :\n";
      Avdevice.get_input_audio_devices() |> List.iter
        (fun d -> printf"\t%s (%s)\n"(get_input_name d)(get_input_long_name d));
      printf "\noutput devices :\n";
      Avdevice.get_output_audio_devices() |> List.iter
        (fun d -> printf"\t%s (%s)\n"(get_output_name d)(get_output_long_name d));
      printf"\nusage: %s input [output]\ninput and output can be devices or file names\n" Sys.argv.(0);
      exit 0));

  let src = try Avdevice.find_input_audio_device Sys.argv.(1)
                |> Av.open_input_format
    with Avutil.Failure _ -> Av.open_input Sys.argv.(1) in

  let (_, ias, _) = Av.find_best_audio_stream src in

  let dst = try
      (if Array.length Sys.argv < 3 then
         List.hd(Avdevice.get_output_audio_devices())
       else Avdevice.find_output_audio_device Sys.argv.(2))
      |> Av.open_output_format
    with Avutil.Failure _ ->
      Av.open_output Sys.argv.(2)
      |> Av.new_audio_stream ~codec_id:Avcodec.Audio.AC_FLAC |> Av.get_output in

  Avdevice.Dev_to_app.(set_control_message_callback (function
      | Volume_level_changed v ->
        Printf.printf "Volume level changed to %f %%\n"(v *. 100.)
      | _ -> print_endline "Unexpected dev to app controle message")) dst;

  (try Avdevice.App_to_dev.(control_messages[Get_volume; Set_volume 0.3]) dst
   with Avutil.Failure msg -> prerr_endline msg);

  let rec run n =
    if n > 0 then match Av.read ias with
      | Av.Frame frame -> Av.write_audio dst frame; run(n - 1)
      | Av.End_of_stream -> ()
  in
  run 500;

  Av.close src;
  Av.close dst;

  Gc.full_major (); Gc.full_major ();
