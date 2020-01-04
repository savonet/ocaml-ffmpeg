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
      Av.write_frame out_stream (Av.read_frame stream);
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
