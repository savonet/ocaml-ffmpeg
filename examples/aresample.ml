let () = Printexc.record_backtrace true

let () =
  if Array.length Sys.argv < 3 then (
    Printf.printf "usage: %s input_file output_file\n" Sys.argv.(0);
    exit 1 );

  Avutil.Log.set_level `Debug;
  Avutil.Log.set_callback print_string;

  let src = Av.open_input Sys.argv.(1) in
  let dst = Av.open_output Sys.argv.(2) in
  let audio_codec = Avcodec.Audio.find_encoder "ac3" in

  let audio_params, audio_input, oass =
    Av.find_best_audio_stream src |> fun (i, audio_input, params) ->
    let opts = Av.mk_audio_opts ~params () in
    (params, audio_input, (i, Av.new_audio_stream ~codec:audio_codec ~opts dst))
  in

  let filter =
    let config = Avfilter.init () in
    let abuffer =
      let time_base = Av.get_time_base audio_input in
      let sample_rate = Avcodec.Audio.get_sample_rate audio_params in
      let sample_format =
        Avutil.Sample_format.get_id
          (Avcodec.Audio.get_sample_format audio_params)
      in
      let channels = Avcodec.Audio.get_nb_channels audio_params in
      let channel_layout =
        Avutil.Channel_layout.get_id
          (Avcodec.Audio.get_channel_layout audio_params)
      in
      let args =
        [
          `Pair ("time_base", `Rational time_base);
          `Pair ("sample_rate", `Int sample_rate);
          `Pair ("sample_fmt", `Int sample_format);
          `Pair ("channels", `Int channels);
          `Pair ("channel_layout", `Int channel_layout);
        ]
      in
      let buffer =
        List.find
          (fun { Avfilter.name; _ } -> name = "abuffer")
          Avfilter.buffers
      in
      {
        Avfilter.node_name = "in";
        node_args = Some args;
        node_pad = List.hd Avfilter.(buffer.io.outputs.audio);
      }
    in
    let outputs = { Avfilter.audio = [abuffer]; video = [] } in
    let sink =
      let sink =
        List.find
          (fun { Avfilter.name; _ } -> name = "abuffersink")
          Avfilter.sinks
      in
      {
        Avfilter.node_name = "out";
        node_args = None;
        node_pad = List.hd Avfilter.(sink.io.inputs.audio);
      }
    in
    let inputs = { Avfilter.audio = [sink]; video = [] } in
    let _ =
      Avfilter.parse { inputs; outputs }
        "aresample=22050,aformat=channel_layouts=stereo" config
    in
    Avfilter.launch config
  in

  let process_audio i frm =
    try
      let stream = List.assoc i [oass] in
      let _, input = List.hd Avfilter.(filter.inputs.audio) in
      input frm;
      let _, output = List.hd Avfilter.(filter.outputs.audio) in
      let rec flush () =
        try
          Av.write_frame stream (output ());
          flush ()
        with Avutil.Error `Eagain -> ()
      in
      flush ()
    with Not_found -> ()
  in

  Gc.full_major ();
  Gc.full_major ();

  src |> Av.iter_input_frame ~audio:process_audio;

  Av.close src;
  Av.close dst;

  Gc.full_major ();
  Gc.full_major ()
