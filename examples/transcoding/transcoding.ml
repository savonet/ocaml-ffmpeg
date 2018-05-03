open FFmpeg

let () =
  if Array.length Sys.argv < 3 then (
    Printf.printf"usage: %s input_file output_file\n" Sys.argv.(0);
    exit 1);

  let src = Av.open_input Sys.argv.(1) in
  let dst = Av.open_output Sys.argv.(2) in

  let oass = Av.get_audio_streams src |> List.fold_left(fun oss (i, _, codec) ->
      (i, Av.new_audio_stream ~codec_id:`Aac ~codec dst)::oss) [] in

  let ovss = Av.get_video_streams src |> List.fold_left(fun oss (i, _, codec) ->
      (i, Av.new_video_stream ~codec_id:`H264 ~codec dst)::oss) [] in

  let osss = Av.get_subtitle_streams src |> List.fold_left(fun oss (i, _, codec) ->
      (i, Av.new_subtitle_stream ~codec dst)::oss) [] in

  src |> Av.iter_input_frame
    ~audio:(fun i frm -> Av.write_frame(List.assoc i oass) frm)
    ~video:(fun i frm -> Av.write_frame(List.assoc i ovss) frm)
    ~subtitle:(fun i frm -> Av.write_frame(List.assoc i osss) frm);

  Av.close src;
  Av.close dst;

  Gc.full_major (); Gc.full_major ()
