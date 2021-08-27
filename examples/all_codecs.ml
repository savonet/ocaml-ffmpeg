let () =
  Printf.printf "====== Audio ======\n%!";
  List.iter
    (fun id ->
      Printf.printf "Available audio codec: %s\n"
        (Avcodec.Audio.string_of_id id))
    Avcodec.Audio.codec_ids;

  List.iter
    (fun c ->
      Printf.printf "Available audio encoder: %s - %s\n%!"
        (Avcodec.Audio.get_name c)
        (Avcodec.Audio.get_description c))
    Avcodec.Audio.encoders;

  List.iter
    (fun c ->
      Printf.printf "Available audio decoder: %s - %s\n%!"
        (Avcodec.Audio.get_name c)
        (Avcodec.Audio.get_description c))
    Avcodec.Audio.decoders;

  Printf.printf "\n\n";

  Printf.printf "====== Video ======\n%!";

  List.iter
    (fun id ->
      Printf.printf "Available video codec: %s\n"
        (Avcodec.Video.string_of_id id))
    Avcodec.Video.codec_ids;

  List.iter
    (fun c ->
      Printf.printf "Available video encoder: %s - %s\n%!"
        (Avcodec.Video.get_name c)
        (Avcodec.Video.get_description c))
    Avcodec.Video.encoders;

  List.iter
    (fun c ->
      Printf.printf "Available video decoder: %s - %s\n%!"
        (Avcodec.Video.get_name c)
        (Avcodec.Video.get_description c))
    Avcodec.Video.decoders;

  Printf.printf "\n\n";

  Printf.printf "====== Subtitle ======\n%!";

  List.iter
    (fun id ->
      Printf.printf "Available subtitle codec: %s\n"
        (Avcodec.Subtitle.string_of_id id))
    Avcodec.Subtitle.codec_ids;

  List.iter
    (fun c ->
      Printf.printf "Available subtitle encoder: %s - %s\n%!"
        (Avcodec.Subtitle.get_name c)
        (Avcodec.Subtitle.get_description c))
    Avcodec.Subtitle.encoders;

  List.iter
    (fun c ->
      Printf.printf "Available subtitle decoder: %s - %s\n%!"
        (Avcodec.Subtitle.get_name c)
        (Avcodec.Subtitle.get_description c))
    Avcodec.Subtitle.decoders
