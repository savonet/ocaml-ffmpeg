open FFmpeg

let () =
  if Array.length Sys.argv < 2 then Printf.(
      printf "\ninput devices :\n";
      Avdevice.get_input_audio_devices() |> List.iter
        (fun d -> printf"\t%s (%s)\n"(Av.get_input_format_name d)(Av.get_input_format_long_name d));
      printf "\noutput devices :\n";
      Avdevice.get_output_audio_devices() |> List.iter
        (fun d -> printf"\t%s (%s)\n"(Av.get_output_format_name d)(Av.get_output_format_long_name d));
      printf"\nusage: %s input [output]\ninput and output can be devices or file names\n" Sys.argv.(0);
      exit 0);

  let src = try Avdevice.get_input_audio_devices()
                |> List.find(fun d -> Av.get_input_format_name d = Sys.argv.(1))
                |> Av.open_input_format
    with Not_found -> Av.open_input Sys.argv.(1) in

  let (_, ias, _) = Av.find_best_audio_stream src in

  let dst, codec_id = try Avdevice.get_output_audio_devices()
                          |> (if Array.length Sys.argv < 3 then List.hd
                              else List.find(fun d -> Av.get_output_format_name d = Sys.argv.(2)))
                          |> fun fmt ->
                          (Av.open_output_format fmt, Av.get_format_audio_codec_id fmt)
    with Not_found ->
      (Av.open_output Sys.argv.(2), Avcodec.Audio.AC_FLAC) in

  let oas = Av.new_audio_stream ~codec_id dst in

  Avdevice.Dev_to_app.(set_control_message_callback (function
      | Volume_level_changed v ->
        Printf.printf "Volume level changed to %f %%\n"(v *. 100.)
      | _ -> print_endline "Unexpected dev to app controle message")) dst;

  (try Avdevice.App_to_dev.(control_messages[Get_volume; Set_volume 0.3]) dst
   with Avutil.Failure msg -> prerr_endline msg) |> ignore;

  let rec run n =
    if n > 0 then match Av.read ias with
      | Av.Frame frame -> Av.write oas frame; run(n - 1)
      | Av.End_of_file -> ()
  in
  run 500;

  Av.close_input src;
  Av.close_output dst;

  Gc.full_major ();
  Gc.full_major ();
