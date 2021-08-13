let () =
  if Array.length Sys.argv < 3 then (
    Printf.eprintf "Usage: %s <input_file> <output_file>.mp4\n" Sys.argv.(0);
    exit 1);

  Avutil.Log.set_level `Debug;
  Avutil.Log.set_callback print_string;

  let src = Av.open_input Sys.argv.(1) in

  let idx, istream, params = Av.find_best_audio_stream src in

  let codec = Avcodec.Audio.find_encoder_by_name "aac" in

  let channel_layout = Avcodec.Audio.get_channel_layout params in
  let sample_format = Avcodec.Audio.get_sample_format params in
  let sample_rate = Avcodec.Audio.get_sample_rate params in
  let time_base = { Avutil.num = 1; den = sample_rate } in

  let ostream =
    Av.open_output Sys.argv.(2)
    |> Av.new_audio_stream ~channel_layout ~sample_format ~sample_rate
         ~time_base ~codec
  in

  let rec f () =
    match Av.read_input ~audio_frame:[istream] src with
      | `Audio_frame (i, frame) when i = idx ->
          Av.write_frame ostream frame;
          f ()
      | exception Avutil.Error `Eof -> ()
      | _ -> f ()
  in
  f ();

  Av.get_input istream |> Av.close;
  Av.get_output ostream |> Av.close;

  Gc.full_major ();
  Gc.full_major ()
