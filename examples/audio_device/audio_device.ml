open FFmpeg
module If = Av.Input.Format
module Of = Av.Output.Format

let () =
  if Array.length Sys.argv < 2 then Printf.(
      printf "\ninput devices :";
      Avdevice.get_input_audio_devices() |> List.iter(fun d -> printf" %s"(If.get_name d));
      printf "\noutput devices :";
      Avdevice.get_output_audio_devices() |> List.iter(fun d -> printf" %s"(Of.get_name d));

      printf"\nusage: %s input [output]\ninput and output can be devices or file names\n" Sys.argv.(0);
      exit 1);

  let src = try Avdevice.get_input_audio_devices()
                |> List.find(fun d -> If.get_name d = Sys.argv.(1))
                |> Av.Input.open_format
    with Not_found -> Av.Input.open_url Sys.argv.(1) in

  let dst, codec_id = try Avdevice.get_output_audio_devices()
                          |> (if Array.length Sys.argv < 3 then List.hd
                              else List.find(fun d -> Of.get_name d = Sys.argv.(2)))
                          |> fun fmt ->
                          (Av.Output.open_format fmt, Of.get_audio_codec fmt)
    with Not_found ->
      (Av.Output.open_file Sys.argv.(2), Avcodec.Audio.AC_FLAC) in

  let sid = Av.Output.new_audio_stream ~codec_id dst in

  Av.(dst-<Avdevice.Dev_to_app.(set_control_message_callback (function
      | Volume_level_changed v ->
        Printf.printf "Volume level changed to %f %%\n"(v *. 100.)
      | _ -> print_endline "Unexpected dev to app controle message")));

  (try
     Av.(dst-<Avdevice.App_to_dev.(control_messages[Get_volume; Set_volume 0.3]))
   with Avutil.Failure msg -> prerr_endline msg) |> ignore;

  let rec run n =
    if n > 0 then match Av.Input.read_audio src with
      | Av.Input.Audio frame -> Av.Output.write_audio_frame dst sid frame;
        run(n - 1)
      | Av.Input.End_of_file -> ()
  in
  run 500;

  Av.Input.close src;
  Av.Output.close dst;

  Gc.full_major ()
