let string_of_properties = function
  | `Intra_only -> "Intra only"
  | `Lossy -> "Lossy"
  | `Lossless -> "Lossless"
  | `Reorder -> "Reorder"
  | `Bitmap_sub -> "Bitmap sub"
  | `Text_sub -> "Text sub"

let string_of_media_type = function
  | `Unknown -> "unknown"
  | `Video -> "video"
  | `Audio -> "audio"
  | `Data -> "data"
  | `Subtitle -> "subtitle"
  | `Attachment -> "attachment"

let print_descriptor = function
  | None -> "(none)\n"
  | Some
      { Avcodec.media_type; name; long_name; mime_types; properties; profiles }
    ->
      Printf.sprintf
        "\n\
        \  Name: %s\n\
        \  Media type: %s\n\
        \  Long name: %s\n\
        \  Mime types: %s\n\
        \  Properties: %s\n\
        \  Profiles:%s\n"
        name
        (string_of_media_type media_type)
        (Option.value ~default:"(none)" long_name)
        (String.concat ", " mime_types)
        (String.concat ", " (List.map string_of_properties properties))
        (String.concat ""
           (List.map
              (fun { Avcodec.id; profile_name } ->
                Printf.sprintf "\n    ID: %i, name: %s" id profile_name)
              profiles))

let () =
  Printf.printf "====== Audio ======\n%!";
  List.iter
    (fun id ->
      Printf.printf "Available audio codec: %s\nDescriptor:%s\n"
        (Avcodec.Audio.string_of_id id)
        (print_descriptor (Avcodec.Audio.descriptor id)))
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
      Printf.printf "Available video codec: %s\nDescriptor:%s\n"
        (Avcodec.Video.string_of_id id)
        (print_descriptor (Avcodec.Video.descriptor id)))
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
      Printf.printf "Available subtitle codec: %s\nDescriptor:%s\n"
        (Avcodec.Subtitle.string_of_id id)
        (print_descriptor (Avcodec.Subtitle.descriptor id)))
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
