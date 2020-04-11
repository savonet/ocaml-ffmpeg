let () =
  if Array.length Sys.argv < 3 then (
    Printf.printf
      "      usage: %s input output\n\
      \      API example program to remux a media file with libavformat and \
       libavcodec.\n\
      \      The output format is guessed according to the file extension.\n"
      Sys.argv.(0);
    exit 0 );

  Avutil.Log.set_level `Debug;
  Avutil.Log.set_callback print_string;

  let src = Av.open_input Sys.argv.(1) in
  let dst = Av.open_output Sys.argv.(2) in

  let oass =
    Av.get_audio_streams src
    |> List.map (fun (i, stream, _) ->
           let params = Av.get_codec_params stream in
           let id = Avcodec.Audio.get_params_id params in
           let codec =
             Avcodec.Audio.find_encoder (Avcodec.Audio.string_of_id id)
           in
           let channel_layout = Avcodec.Audio.get_channel_layout params in
           let channels = Avcodec.Audio.get_nb_channels params in
           let sample_format = Avcodec.Audio.get_sample_format params in
           let sample_rate = Avcodec.Audio.get_sample_rate params in
           let time_base = { Avutil.num = 1; den = sample_rate } in
           ( i,
             Av.new_audio_stream ~channels ~channel_layout ~sample_format
               ~sample_rate ~time_base ~codec dst ))
  in

  let ovss =
    Av.get_video_streams src
    |> List.map (fun (i, stream, _) ->
           let params = Av.get_codec_params stream in
           let id = Avcodec.Video.get_params_id params in
           let codec =
             Avcodec.Video.find_encoder (Avcodec.Video.string_of_id id)
           in
           let width = Avcodec.Video.get_width params in
           let height = Avcodec.Video.get_height params in
           let pixel_format = Avcodec.Video.get_pixel_format params in
           let frame_rate = { Avutil.num = 25; den = 1 } in
           let time_base = { Avutil.num = 1; den = 25 } in
           ( i,
             Av.new_video_stream ~pixel_format ~frame_rate ~time_base ~width
               ~height ~codec dst ))
  in

  let osss =
    Av.get_subtitle_streams src
    |> List.map (fun (i, stream, _) ->
           let params = Av.get_codec_params stream in
           let id = Avcodec.Subtitle.get_params_id params in
           let codec =
             Avcodec.Subtitle.find_encoder (Avcodec.Subtitle.string_of_id id)
           in
           let time_base = { Avutil.num = 1; den = 25 } in
           (i, Av.new_subtitle_stream ~time_base ~codec dst))
  in

  src
  |> Av.iter_input_packet
       ~audio:(fun i pkt -> Av.write_packet (List.assoc i oass) pkt)
       ~video:(fun i pkt -> Av.write_packet (List.assoc i ovss) pkt)
       ~subtitle:(fun i pkt -> Av.write_packet (List.assoc i osss) pkt);

  Av.close src;
  Av.close dst;

  Gc.full_major ();
  Gc.full_major ()
