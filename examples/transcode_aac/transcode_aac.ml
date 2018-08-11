open FFmpeg

let () =
  if Array.length Sys.argv < 3 then (
    Printf.eprintf "Usage: %s <input_file> <output_file>.mp4\n" Sys.argv.(0);
    exit 1);

  Avutil.Log.set_level `Debug;
  Avutil.Log.set_callback print_string;
  
  let _, is, codec = Av.open_input Sys.argv.(1) |> Av.find_best_audio_stream in

  let os = Av.open_output Sys.argv.(2)
           |> Av.new_audio_stream ~codec_id:`Aac ~codec in

  is |> Av.iter_frame(fun frm -> Av.write_frame os frm);

  Av.get_input is |> Av.close;
  Av.get_output os |> Av.close;

  Gc.full_major (); Gc.full_major ()
