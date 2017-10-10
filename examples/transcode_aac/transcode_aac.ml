open FFmpeg

let () =
  if Array.length Sys.argv < 3 then (
    Printf.eprintf "Usage: %s <input_file> <output_file>.mp4\n" Sys.argv.(0);
    exit 1);

  let src = Av.open_input Sys.argv.(1) in
  let _, is, codec = Av.find_best_audio_stream src in

  let dst = Av.open_output Sys.argv.(2) in

  let os = Av.new_audio_stream ~codec_id:Avcodec.Audio.AC_AAC ~codec dst in

  Av.iter(fun frm -> Av.write os frm) is;

  Av.close_input src;
  Av.close_output dst;

  Gc.full_major ()
