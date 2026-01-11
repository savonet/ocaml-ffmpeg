open Avutil
open Printf

let test_color_properties () =
  let cs = `Bt709 in
  let cs_name = Color_space.name cs in
  printf "Color_space test: %s -> %s\n" "Bt709" cs_name;
  (match Color_space.from_name cs_name with
    | Some cs' when cs = cs' -> printf "Color_space from_name: OK\n"
    | _ -> printf "Color_space from_name: FAILED\n");

  let cr = `Mpeg in
  let cr_name = Color_range.name cr in
  printf "Color_range test: %s -> %s\n" "Mpeg" cr_name;
  (match Color_range.from_name cr_name with
    | Some cr' when cr = cr' -> printf "Color_range from_name: OK\n"
    | _ -> printf "Color_range from_name: FAILED\n");

  let cp = `Bt709 in
  let cp_name = Color_primaries.name cp in
  printf "Color_primaries test: %s -> %s\n" "Bt709" cp_name;
  (match Color_primaries.from_name cp_name with
    | Some cp' when cp = cp' -> printf "Color_primaries from_name: OK\n"
    | _ -> printf "Color_primaries from_name: FAILED\n");

  let ct = `Bt709 in
  let ct_name = Color_trc.name ct in
  printf "Color_trc test: %s -> %s\n" "Bt709" ct_name;
  (match Color_trc.from_name ct_name with
    | Some ct' when ct = ct' -> printf "Color_trc from_name: OK\n"
    | _ -> printf "Color_trc from_name: FAILED\n");

  let cl = `Left in
  let cl_name = Chroma_location.name cl in
  printf "Chroma_location test: %s -> %s\n" "Left" cl_name;
  (match Chroma_location.from_name cl_name with
    | Some cl' when cl = cl' -> printf "Chroma_location from_name: OK\n"
    | _ -> printf "Chroma_location from_name: FAILED\n");
  printf "\n"

let test_file_info url =
  let input = Av.open_input url in
  printf "%s (%s s) :\n" url
    (match Av.get_input_duration input with
      | None -> "N/A"
      | Some d -> Int64.to_string d);
  Av.get_input_metadata input
  |> List.iter (fun (k, v) -> printf "\t%s : %s\n" k v);
  Av.get_audio_streams input
  |> List.iter (fun (idx, stm, cd) ->
      Av.get_metadata stm |> List.iter (fun (k, v) -> printf "\t%s : %s\n" k v);
      Avcodec.Audio.(
        printf "\tAudio stream %d : %s %s, %s %s, %s %d, %s %d, %s %d, %s %Ld\n"
          idx "codec"
          (get_params_id cd |> string_of_id)
          "sample format"
          ( get_sample_format cd |> fun p ->
            Option.get (Sample_format.get_name p) )
          "channels" (get_nb_channels cd) "bit rate" (get_bit_rate cd)
          "sample rate" (get_sample_rate cd) "duration (ms)"
          (Av.get_duration ~format:`Millisecond stm)));
  Av.get_video_streams input
  |> List.iter (fun (idx, stm, cd) ->
      Av.get_metadata stm |> List.iter (fun (k, v) -> printf "\t%s : %s\n" k v);
      Avcodec.Video.(
        let sar = get_sample_aspect_ratio cd in
        printf
          "\tVideo stream %d : %s %s, %s %d, %s %d, %s %d / %d, %s %d, %s %Ld\n"
          idx "codec"
          (get_params_id cd |> string_of_id)
          "width" (get_width cd) "height" (get_height cd) "sample aspect ratio"
          sar.num sar.den "bit rate" (get_bit_rate cd) "duration (ns)"
          (Av.get_duration ~format:`Nanosecond stm);
        let decoder =
          create_decoder ~params:cd (find_decoder (get_params_id cd))
        in
        (try
           let rec read_frame () =
             match Av.read_input ~video_frame:[stm] input with
               | `Video_frame (_, frame) ->
                   let cs = Video.frame_get_color_space frame in
                   let cr = Video.frame_get_color_range frame in
                   let cp = Video.frame_get_color_primaries frame in
                   let ct = Video.frame_get_color_trc frame in
                   let cl = Video.frame_get_chroma_location frame in
                   printf
                     "\t\tFirst frame color_space: %s, color_range: %s, \
                      color_primaries: %s, color_trc: %s, chroma_location: %s\n"
                     (Color_space.name cs) (Color_range.name cr)
                     (Color_primaries.name cp) (Color_trc.name ct)
                     (Chroma_location.name cl)
               | exception Avutil.Error `Eof -> ()
               | _ -> read_frame ()
           in
           read_frame ()
         with _ -> printf "\t\tCould not read video frame\n");
        ignore decoder));
  Av.get_subtitle_streams input
  |> List.iter (fun (idx, stm, cd) ->
      Av.get_metadata stm |> List.iter (fun (k, v) -> printf "\t%s : %s\n" k v);
      Avcodec.Subtitle.(
        printf "\tSubtitle stream %d : %s %s, %s %Ld\n" idx "codec"
          (get_params_id cd |> string_of_id)
          "duration (us)"
          (Av.get_duration ~format:`Microsecond stm)));
  printf "\n"

let () =
  Avutil.Log.set_level `Debug;
  Avutil.Log.set_callback print_string;

  test_color_properties ();
  Sys.argv |> Array.to_list |> List.tl |> List.iter test_file_info
