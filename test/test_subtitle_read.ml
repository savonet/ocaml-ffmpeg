(* Simple test to read subtitles from a file *)

open Avutil

let () =
  if Array.length Sys.argv < 2 then (
    Printf.eprintf "Usage: %s input_file\n" Sys.argv.(0);
    exit 1);

  let input_file = Sys.argv.(1) in
  Printf.printf "Opening: %s\n%!" input_file;

  let src = Av.open_input input_file in

  (* Get subtitle streams *)
  let subtitle_streams = Av.get_subtitle_streams src in
  Printf.printf "Found %d subtitle stream(s)\n%!" (List.length subtitle_streams);

  List.iter
    (fun (idx, _stream, params) ->
      let codec_id = Avcodec.Subtitle.get_params_id params in
      Printf.printf "  Stream %d: codec=%s\n%!" idx
        (Avcodec.Subtitle.string_of_id codec_id))
    subtitle_streams;

  if List.length subtitle_streams = 0 then (
    Printf.printf "No subtitles found.\n";
    Av.close src;
    exit 0);

  (* Read subtitle frames *)
  let count = ref 0 in
  let rec read_loop () =
    match
      Av.read_input
        ~subtitle_frame:(List.map (fun (_, s, _) -> s) subtitle_streams)
        src
    with
      | `Subtitle_frame (i, frame) ->
          incr count;
          let start_time, end_time, lines = Subtitle.frame_to_lines frame in
          Printf.printf "[%d] Stream %d: %.2fs - %.2fs\n%!" !count i start_time
            end_time;
          List.iter (fun line -> Printf.printf "    %s\n%!" line) lines;
          read_loop ()
      | exception Error `Eof ->
          Printf.printf "End of file. Read %d subtitle(s).\n%!" !count
      | exception Error err ->
          Printf.eprintf "Error: %s\n%!" (string_of_error err)
      | _ ->
          (* Skip other stream types *)
          read_loop ()
  in
  read_loop ();

  Av.close src;
  Gc.full_major ()
