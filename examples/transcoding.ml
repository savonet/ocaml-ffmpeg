let () = Printexc.record_backtrace true

let () =
  if Array.length Sys.argv < 3 then (
    Printf.printf "usage: %s input_file output_file\n" Sys.argv.(0);
    exit 1 );

  Avutil.Log.set_level `Debug;
  Avutil.Log.set_callback print_string;

  let src = Av.open_input Sys.argv.(1) in
  let dst = Av.open_output Sys.argv.(2) in
  let audio_codec = Avcodec.Audio.find_encoder "aac" in
  let video_codec = Avcodec.Video.find_encoder "mpeg4" in

  let oass =
    Av.get_audio_streams src
    |> List.map (fun (i, _, params) ->
           let channel_layout = Avcodec.Audio.get_channel_layout params in
           let sample_format = Avcodec.Audio.get_sample_format params in
           let sample_rate = Avcodec.Audio.get_sample_rate params in
           let time_base = { Avutil.num = 1; den = sample_rate } in
           ( i,
             Av.new_audio_stream ~channel_layout ~sample_format ~sample_rate
               ~time_base ~codec:audio_codec dst ))
  in

  let frame_rate = { Avutil.num = 25; den = 1 } in
  let time_base = { Avutil.num = 1; den = 25 } in

  let ovss =
    Av.get_video_streams src
    |> List.map (fun (i, _, params) ->
           let width = Avcodec.Video.get_width params in
           let height = Avcodec.Video.get_height params in
           let pixel_format = Avcodec.Video.get_pixel_format params in
           ( i,
             Av.new_video_stream ~pixel_format ~frame_rate ~time_base ~width
               ~height ~codec:video_codec dst ))
  in

  let osss =
    Av.get_subtitle_streams src
    |> List.map (fun (i, _, params) ->
           let codec =
             Avcodec.Subtitle.find_encoder
               (Avcodec.Subtitle.string_of_id
                  (Avcodec.Subtitle.get_params_id params))
           in
           (i, Av.new_subtitle_stream ~time_base ~codec dst))
  in

  src
  |> Av.iter_input_frame
       ~audio:(fun i frm -> Av.write_frame (List.assoc i oass) frm)
       ~video:(fun i frm -> Av.write_frame (List.assoc i ovss) frm)
       ~subtitle:(fun i frm -> Av.write_frame (List.assoc i osss) frm);

  Av.close src;
  Av.close dst;

  Gc.full_major ();
  Gc.full_major ()
