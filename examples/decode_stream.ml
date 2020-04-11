let () = Printexc.record_backtrace true

let () =
  if Array.length Sys.argv < 4 then (
    Printf.eprintf
      "      usage: %s <input file> <output file> <output codec>\n\
      \      API example program to show how to read audio frames from an \
       input file using the streaming API.\n\
      \      This program parse data to packets from a file, decodes them, and \
       writes decoded\n\
      \      audio frames to a audio file named <output file>.\n"
      Sys.argv.(0);
    exit 1 );

  Avutil.Log.set_callback (fun _ -> ());

  let in_fd = Unix.openfile Sys.argv.(1) [Unix.O_RDONLY] 0 in
  let out_file = Av.open_output Sys.argv.(2) in
  let codec = Avcodec.Audio.find_encoder Sys.argv.(3) in
  let out_stream = Av.new_audio_stream ~codec out_file in

  let frame_size =
    if List.mem `Variable_frame_size (Avcodec.Audio.capabilities codec) then 512
    else Av.get_frame_size out_stream
  in

  let filters = ref None in
  let get_filters frame =
    match !filters with
      | Some f -> f
      | None ->
          let sample_rate = Avutil.Audio.frame_get_sample_rate frame in
          let time_base = { Avutil.num = 1; den = sample_rate } in
          let channels = Avutil.Audio.frame_get_channels frame in
          let channel_layout = Avutil.Audio.frame_get_channel_layout frame in
          let sample_format = Avutil.Audio.frame_get_sample_format frame in
          let f =
            Avfilter.Utils.split_frame_size ~sample_rate ~time_base ~channels
              ~channel_layout ~sample_format frame_size
          in
          filters := Some f;
          f
  in

  let pts = ref 0L in
  let rec flush filter_out =
    try
      filter_out () |> fun frame ->
      Avutil.frame_set_pts frame (Some !pts);
      pts := Int64.add !pts (Int64.of_int (Avutil.Audio.frame_nb_samples frame));
      Av.write_frame out_stream frame;
      flush filter_out
    with Avutil.Error `Eagain -> ()
  in

  let write_frame frame =
    let filter_in, filter_out = get_filters frame in
    filter_in frame;
    flush filter_out
  in

  let read = Unix.read in_fd in
  let seek = Unix.lseek in_fd in
  let container = Av.open_input_stream ~seek read in
  let _, stream, params = Av.find_best_audio_stream container in
  let codec_id = Avcodec.Audio.get_params_id params in
  let sample_format =
    match Avcodec.Audio.get_sample_format params with
      | `Dbl -> "dbl"
      | `Dblp -> "dblp"
      | `Flt -> "flt"
      | `Fltp -> "fltp"
      | `None -> "none"
      | `S16 -> "s16"
      | `S16p -> "s16p"
      | `S32 -> "s32"
      | `S32p -> "s32p"
      | `S64 -> "s64"
      | `S64p -> "s64p"
      | `U8 -> "u8"
      | `U8p -> "u8p"
  in
  Printf.printf "Detected format: %s, %dHz, %d channels, %s\n%!"
    (Avcodec.Audio.string_of_id codec_id)
    (Avcodec.Audio.get_sample_rate params)
    (Avcodec.Audio.get_nb_channels params)
    sample_format;
  let rec f () =
    try
      write_frame (Av.read_frame stream);
      f ()
    with
      | Avutil.Error `Eof -> ()
      | Avutil.Error `Invalid_data -> f ()
  in
  f ();

  Unix.close in_fd;
  Av.close container;
  Av.close out_file;

  Gc.full_major ();
  Gc.full_major ()
