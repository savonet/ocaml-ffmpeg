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
           let opts = Av.mk_audio_opts ~params () in
           (i, Av.new_audio_stream ~codec:audio_codec ~opts dst))
  in

  let ovss =
    Av.get_video_streams src
    |> List.map (fun (i, _, params) ->
           let opts = Av.mk_video_opts ~params () in
           (i, Av.new_video_stream ~codec:video_codec ~opts dst))
  in

  let osss =
    Av.get_subtitle_streams src
    |> List.map (fun (i, _, params) ->
           let codec =
             Avcodec.Subtitle.find_encoder
               (Avcodec.Subtitle.string_of_id
                  (Avcodec.Subtitle.get_params_id params))
           in
           (i, Av.new_subtitle_stream ~codec dst))
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
