open FFmpeg

let () =
  if Array.length Sys.argv < 3 then (
    Printf.eprintf "Usage: %s <input_file> <output_file>.mp4\n" Sys.argv.(0);
    exit 1);

  Avutil.Log.set_level `Debug;
  Avutil.Log.set_callback print_string;
  
  let _, istream, codec = Av.open_input Sys.argv.(1)
                          |> Av.find_best_audio_stream in

  let ostream = Av.open_output Sys.argv.(2)
           |> Av.new_audio_stream ~codec_id:`Aac ~codec in

  istream |> Av.iter_frame (Av.write_frame ostream);

  Av.get_input istream |> Av.close;
  Av.get_output ostream |> Av.close;

  Gc.full_major (); Gc.full_major ()
