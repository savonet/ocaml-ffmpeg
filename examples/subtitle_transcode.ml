(** Subtitle transcoding example.

    This example demonstrates how to: 1. Read subtitles from an input file (MKV,
    MP4, etc.) 2. Decode subtitle frames 3. Re-encode subtitles to a different
    format (e.g., SRT, WebVTT)

    Usage: subtitle_transcode input_file output_file [codec_name]

    Examples: subtitle_transcode movie.mkv subs.srt subrip subtitle_transcode
    movie.mkv subs.vtt webvtt subtitle_transcode movie.mkv movie_subs.mkv ass

    Note: Not all subtitle format conversions are lossless. Text-based subtitles
    (SRT, ASS, WebVTT) generally convert well between each other, but bitmap
    subtitles (DVD, Blu-ray) require OCR for text conversion. *)

open Avutil

let () = Printexc.record_backtrace true

let () =
  if Array.length Sys.argv < 3 then (
    Printf.eprintf
      "Usage: %s input_file output_file [codec_name]\n\n\
       Transcode subtitles from input to output.\n\n\
       Arguments:\n\
      \  input_file   - Input file containing subtitles\n\
      \  output_file  - Output file (format determined by extension)\n\
      \  codec_name   - Optional: encoder name (e.g., subrip, webvtt, ass)\n\n\
       Examples:\n\
      \  %s movie.mkv subtitles.srt subrip\n\
      \  %s movie.mkv subtitles.vtt webvtt\n"
      Sys.argv.(0) Sys.argv.(0) Sys.argv.(0);
    exit 1)

let () =
  Log.set_level `Warning;
  Log.set_callback print_string;

  let input_file = Sys.argv.(1) in
  let output_file = Sys.argv.(2) in

  (* Open input and output containers *)
  let src = Av.open_input input_file in
  let dst = Av.open_output output_file in

  (* Get all subtitle streams from input *)
  let input_subtitle_streams = Av.get_subtitle_streams src in

  if List.length input_subtitle_streams = 0 then (
    Printf.eprintf "No subtitle streams found in %s\n" input_file;
    Av.close src;
    exit 1);

  Printf.printf "Found %d subtitle stream(s) in input\n"
    (List.length input_subtitle_streams);

  (* Create output subtitle streams *)
  let time_base = Subtitle.time_base () in
  let output_streams =
    input_subtitle_streams
    |> List.map (fun (i, _stream, params) ->
        let input_codec_id = Avcodec.Subtitle.get_params_id params in
        Printf.printf "  Stream %d: %s\n" i
          (Avcodec.Subtitle.string_of_id input_codec_id);

        (* Determine output codec *)
        let codec =
          if Array.length Sys.argv > 3 then
            (* Use specified codec *)
            Avcodec.Subtitle.find_encoder_by_name Sys.argv.(3)
          else (
            (* Try to use same codec as input, or fall back to subrip *)
            try Avcodec.Subtitle.find_encoder input_codec_id
            with _ -> Avcodec.Subtitle.find_encoder_by_name "subrip")
        in
        Printf.printf "    -> Encoding to: %s\n"
          (Avcodec.Subtitle.get_name codec);

        (i, Av.new_subtitle_stream ~time_base ~codec dst))
  in

  Printf.printf "\nTranscoding subtitles...\n%!";

  let subtitle_count = ref 0 in

  (* Read and transcode subtitle frames *)
  let rec process () =
    match
      Av.read_input
        ~subtitle_frame:(List.map (fun (_, s, _) -> s) input_subtitle_streams)
        src
    with
      | `Subtitle_frame (i, frame) ->
          incr subtitle_count;

          (* Extract subtitle text for display *)
          let start_time, end_time, lines = Subtitle.frame_to_lines frame in
          if !subtitle_count <= 5 then
            Printf.printf "  [%d] %.2fs - %.2fs: %s\n" !subtitle_count
              start_time end_time
              (String.concat " | " lines);

          (* Write to output stream *)
          (try Av.write_frame (List.assoc i output_streams) frame
           with Error err ->
             Printf.eprintf "Warning: Failed to write subtitle %d: %s\n"
               !subtitle_count (string_of_error err));
          process ()
      | exception Error `Eof -> ()
      | exception Error err ->
          Printf.eprintf "Error reading input: %s\n" (string_of_error err)
      | _ ->
          (* Skip non-subtitle content *)
          process ()
  in
  process ();

  if !subtitle_count > 5 then
    Printf.printf "  ... and %d more\n" (!subtitle_count - 5);

  Printf.printf "\nTranscoded %d subtitle(s)\n" !subtitle_count;

  (* Clean up *)
  Av.close src;
  Av.close dst;

  (* Normalize line endings (FFmpeg produces CRLF, we want LF) *)
  let normalize_line_endings filename =
    let ic = open_in_bin filename in
    let len = in_channel_length ic in
    let content = really_input_string ic len in
    close_in ic;
    let normalized = String.split_on_char '\r' content |> String.concat "" in
    if normalized <> content then begin
      let oc = open_out_bin filename in
      output_string oc normalized;
      close_out oc
    end
  in
  normalize_line_endings output_file;

  Printf.printf "Output written to: %s\n" output_file;

  (* Force garbage collection to test for memory leaks *)
  Gc.full_major ();
  Gc.full_major ()
