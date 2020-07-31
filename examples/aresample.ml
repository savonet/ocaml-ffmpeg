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

  let audio_params, audio_input, idx, oass =
    Av.find_best_audio_stream src |> fun (i, audio_input, params) ->
    let channel_layout = Avcodec.Audio.get_channel_layout params in
    let channels = Avcodec.Audio.get_nb_channels params in
    let sample_format = Avcodec.Audio.get_sample_format params in
    let sample_rate = Avcodec.Audio.get_sample_rate params in
    let time_base = { Avutil.num = 1; den = sample_rate } in
    ( params,
      audio_input,
      i,
      Av.new_audio_stream ~channels ~channel_layout ~sample_format ~sample_rate
        ~time_base ~codec:audio_codec dst )
  in

  let frame_size =
    if List.mem `Variable_frame_size (Avcodec.Audio.capabilities audio_codec)
    then 512
    else Av.get_frame_size oass
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
      {
        Avfilter.node_name = "in";
        node_args = Some args;
        node_pad = List.hd Avfilter.(abuffer.io.outputs.audio);
      }
    in
    let outputs = { Avfilter.audio = [abuffer]; video = [] } in
    let sink =
      {
        Avfilter.node_name = "out";
        node_args = None;
        node_pad = List.hd Avfilter.(abuffersink.io.inputs.audio);
      }
    in
    let inputs = { Avfilter.audio = [sink]; video = [] } in
    let _ =
      Avfilter.parse { inputs; outputs }
        "aresample=22050,aformat=channel_layouts=stereo" config
    in
    Avfilter.launch config
  in

  let _, output = List.hd Avfilter.(filter.outputs.audio) in
  let context = output.context in
  Avfilter.set_frame_size context frame_size;
  let time_base = Avfilter.time_base context in
  Printf.printf
    "Sink info:\n\
     time_base: %d/%d\n\
     channels: %d\n\
     channel_layout: %s\n\
     sample_rate: %d\n"
    time_base.Avutil.num time_base.Avutil.den
    (Avfilter.channels context)
    (Avutil.Channel_layout.get_description (Avfilter.channel_layout context))
    (Avfilter.sample_rate context);

  let process_audio i frm =
    try
      assert (i = idx);
      let _, input = List.hd Avfilter.(filter.inputs.audio) in
      input frm;
      let rec flush () =
        try
          Av.write_frame oass (output.handler ());
          flush ()
        with Avutil.Error `Eagain -> ()
      in
      flush ()
    with Not_found -> ()
  in

  Gc.full_major ();
  Gc.full_major ();

  let rec f () =
    match Av.read_input ~audio_frame:[audio_input] src with
      | `Audio_frame (i, frame) ->
          process_audio i frame;
          f ()
      | exception Avutil.Error `Eof -> ()
      | _ -> f ()
  in
  f ();

  Av.close src;
  Av.close dst;

  Gc.full_major ();
  Gc.full_major ()
