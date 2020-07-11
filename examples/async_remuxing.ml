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
           (i, Av.new_stream_copy ~params dst))
  in

  let ovss =
    Av.get_video_streams src
    |> List.map (fun (i, stream, _) ->
           let params = Av.get_codec_params stream in
           (i, Av.new_stream_copy ~params dst))
  in

  let osss =
    Av.get_subtitle_streams src
    |> List.map (fun (i, stream, _) ->
           let params = Av.get_codec_params stream in
           (i, Av.new_stream_copy ~params dst))
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
