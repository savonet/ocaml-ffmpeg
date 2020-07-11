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

  let rec f () =
    match Av.read_input_packet src with
      | `Audio (i, p) ->
          Av.write_packet (List.assoc i oass) p;
          f ()
      | `Video (i, p) ->
          Av.write_packet (List.assoc i ovss) p;
          f ()
      | `Subtitle (i, p) ->
          Av.write_packet (List.assoc i osss) p;
          f ()
      | exception Avutil.Error `Eof -> ()
  in
  f ();

  Av.close src;
  Av.close dst;

  Gc.full_major ();
  Gc.full_major ()
