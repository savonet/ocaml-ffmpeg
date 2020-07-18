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

  let iass = Av.get_audio_streams src in

  let oass =
    iass
    |> List.map (fun (i, stream, _) ->
           let params = Av.get_codec_params stream in
           (i, Av.new_stream_copy ~params dst))
  in

  let ivss = Av.get_video_streams src in

  let ovss =
    ivss
    |> List.map (fun (i, stream, _) ->
           let params = Av.get_codec_params stream in
           (i, Av.new_stream_copy ~params dst))
  in

  let isss = Av.get_subtitle_streams src in

  let osss =
    isss
    |> List.map (fun (i, stream, _) ->
           let params = Av.get_codec_params stream in
           (i, Av.new_stream_copy ~params dst))
  in

  let rec f () =
    match
      Av.read_input
        ~audio_packet:(List.map (fun (_, s, _) -> s) iass)
        ~video_packet:(List.map (fun (_, s, _) -> s) ivss)
        ~subtitle_packet:(List.map (fun (_, s, _) -> s) isss)
        src
    with
      | `Audio_packet (i, pkt) ->
          let time_base = Av.get_time_base (List.assoc i oass) in
          Av.write_packet (List.assoc i oass) time_base pkt;
          f ()
      | `Video_packet (i, pkt) ->
          let time_base = Av.get_time_base (List.assoc i ovss) in
          Av.write_packet (List.assoc i ovss) time_base pkt;
          f ()
      | `Subtitle_packet (i, pkt) ->
          let time_base = Av.get_time_base (List.assoc i osss) in
          Av.write_packet (List.assoc i osss) time_base pkt;
          f ()
      | exception Avutil.Error `Eof -> ()
      | _ -> assert false
  in
  f ();

  Av.close src;
  Av.close dst;

  Gc.full_major ();
  Gc.full_major ()
