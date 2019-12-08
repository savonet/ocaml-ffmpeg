open FFmpeg

let () =
  Printexc.record_backtrace true

let () =
  if Array.length Sys.argv < 3 then (
    Printf.printf"usage: %s input_file output_file\n" Sys.argv.(0);
    exit 1);

  Avutil.Log.set_level `Debug;
  Avutil.Log.set_callback print_string;
  
  let src = Av.open_input Sys.argv.(1) in
  let dst = Av.open_output Sys.argv.(2) in
  let audio_codec = Avcodec.Audio.find_encoder "aac" in
  let video_codec = Avcodec.Video.find_encoder "mpeg4" in

  let audio_params, audio_input, oass = Av.find_best_audio_stream src |> fun (i, audio_input, params) ->
      let opts = Av.mk_audio_opts ~params () in
      params, audio_input, (i, Av.new_audio_stream ~codec:audio_codec ~opts dst) in

  let video_params, video_input, ovss = Av.find_best_video_stream src |> fun (i, video_input, params) ->
      let opts = Av.mk_video_opts ~params () in
      params, video_input, (i, Av.new_video_stream ~codec:video_codec ~opts dst) in

  let filter =
    let config = Avfilter.init () in
    let buffer =
      let time_base =
        Av.get_time_base video_input
      in
      let pixel_aspect =
        Av.get_pixel_aspect video_input
      in
      let pixel_format =
        Avcodec.Video.get_pixel_format video_params
      in
      let width =
        Avcodec.Video.get_width video_params
      in
      let height =
        Avcodec.Video.get_height video_params
      in
      let args = [
        `Pair ("video_size", `String (Printf.sprintf "%dx%x" width height));
        `Pair ("pix_fmt", `String (Avutil.Pixel_format.to_string pixel_format));
        `Pair ("pixel_aspect", `Rational pixel_aspect);
        `Pair ("time_base", `Rational time_base)
      ] in
     let buffer = List.find (fun ({Avfilter.name}) ->
       name = "buffer") Avfilter.buffers
     in
     FFmpeg.Avfilter.attach ~args ~name:"buffer" buffer config
   in
   let fps =
     let args = [`Pair ("fps", `Int 25)] in
     let buffer = List.find (fun ({Avfilter.name}) ->
       name = "fps") Avfilter.filters
     in
     FFmpeg.Avfilter.attach ~args ~name:"fps" buffer config
   in
   let sink =
     let sink = List.find (fun ({Avfilter.name}) ->
       name = "buffersink") Avfilter.sinks
     in
     FFmpeg.Avfilter.attach ~name:"sink" sink config
   in
   Avfilter.link
        (List.hd Avfilter.(buffer.io.outputs.video))
        (List.hd Avfilter.(fps.io.inputs.video));
   Avfilter.link
        (List.hd Avfilter.(fps.io.outputs.video))
        (List.hd Avfilter.(sink.io.inputs.video));
(*
   let abuffer =
     let time_base =
        Av.get_time_base audio_input
     in
     let sample_rate =
       Avcodec.Audio.get_sample_rate audio_params
     in
     let sample_format =
       Avcodec.Audio.get_sample_format audio_params
     in
     let channels =
       Avcodec.Audio.get_nb_channels audio_params
     in
     let args = [
       `Pair ("time_base", `Rational time_base);
       `Pair ("sample_rate", `Int sample_rate);
       `Pair ("sample_fmt", `String (Avutil.Sample_format.get_name sample_format));
       `Pair ("channels", `Int channels)
     ] in
     let abuffer = List.find (fun ({Avfilter.name}) ->
       name = "abuffer") Avfilter.buffers
     in
     FFmpeg.Avfilter.attach ~args ~name:"abuffer" abuffer config
   in
   let aresample =
     let args = [`Key "22050"] in
     let aresample = List.find (fun ({Avfilter.name}) ->
       name = "aresample") Avfilter.filters
     in
     FFmpeg.Avfilter.attach ~args ~name:"aresample" aresample config
   in
   let asink =
     let asink = List.find (fun ({Avfilter.name}) ->
       name = "abuffersink") Avfilter.sinks
     in
     FFmpeg.Avfilter.attach ~name:"asink" asink config
   in
   Avfilter.link
        (List.hd Avfilter.(abuffer.io.outputs.audio))
        (List.hd Avfilter.(aresample.io.inputs.audio));
   Avfilter.link
        (List.hd Avfilter.(aresample.io.outputs.audio))
        (List.hd Avfilter.(asink.io.inputs.audio));
*)
   Avfilter.launch config
  in

  let process_video i frm =
   try
    let stream = List.assoc i [ovss] in
    let _, input =
      List.hd Avfilter.(filter.inputs.video)
    in
    input frm;
    let _, output =
      List.hd Avfilter.(filter.outputs.video)
    in
    let rec flush () =
      try
        Av.write_frame stream (output ());
        flush ()
      with FFmpeg.Avutil.Error `Eagain -> ()
    in
    flush ()
   with Not_found -> ()
  in

  let process_audio i frm =
   try
    let stream = List.assoc i [oass] in
    Av.write_frame stream frm
   with Not_found -> ()
  in

  src |> Av.iter_input_frame
    ~audio:process_audio
    ~video:process_video;

  Av.close src;
  Av.close dst;

  Gc.full_major (); Gc.full_major ()
