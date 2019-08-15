open FFmpeg
open Avcodec

let () =
  if Array.length Sys.argv < 4 then (
    Printf.eprintf "      usage: %s <input file> <output file> <output codec>
      API example program to show how to read audio frames from an input file using the streaming API.
      This program parse data to packets from a file, decodes them, and writes decoded
      audio frames to a audio file named <output file>.\n" Sys.argv.(0);
    exit 1);

  Avutil.Log.set_callback (fun _ -> ());
  
  let in_fd = Unix.openfile Sys.argv.(1) [Unix.O_RDONLY] 0 in

  let out_file = Av.open_output Sys.argv.(2) in
  let out_stream = Av.new_audio_stream ~codec_name:Sys.argv.(3) out_file in

  let read = Unix.read in_fd in
  let seek = Some (Unix.lseek in_fd) in
  let container =
    Av.open_input_stream {Av.read;seek}
  in
  let (_, stream, _) =
    Av.find_best_audio_stream container
  in
  let rec f () =
    try
      match Av.read_frame stream with
        | `Frame frame ->
             Av.write_frame out_stream frame;
             f ()
        | `End_of_file -> ()
    with
      | FFmpeg.Avutil.Failure s when s = "Failed to read stream : Invalid data found when processing input" ->
          f ()
  in
  f ();

  Unix.close in_fd;
  Av.close out_file;

  Gc.full_major (); Gc.full_major ();
