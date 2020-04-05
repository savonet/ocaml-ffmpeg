open Avfilter

let () = Printexc.record_backtrace true

let string_of_pad { audio; video } =
  let audio =
    String.concat "\n"
      (List.map
         (fun pad -> Printf.sprintf "- name: %s, type: audio" (pad_name pad))
         audio)
  in

  let video =
    String.concat "\n"
      (List.map
         (fun pad -> Printf.sprintf "- name: %s, type: video" (pad_name pad))
         video)
  in

  let audio = if audio = "" then [] else ["\n"; audio] in
  let video = if video = "" then [] else ["\n"; video] in

  String.concat "" (audio @ video)

let () =
  let print_filters cat =
    List.iter (fun { name; description; io } ->
        let { inputs; outputs } = io in
        Printf.printf "%s name: %s\ndescription: %s\ninputs:%s\noutputs:%s\n\n"
          cat name description (string_of_pad inputs) (string_of_pad outputs))
  in

  Printf.printf "## Buffers (inputs) ##\n\n";
  print_filters "Buffer" [abuffer];
  print_filters "Buffer" [buffer];

  Printf.printf "## Sinks (oututs):\n\n";
  print_filters "Sink" [abuffersink];
  print_filters "Sink" [buffersink];

  Printf.printf "## Filters ##\n\n";
  print_filters "Filter" filters;

  Gc.full_major ()
