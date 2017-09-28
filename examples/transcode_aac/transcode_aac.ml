open FFmpeg

let () =
  if Array.length Sys.argv < 3 then (
    Printf.eprintf "Usage: %s <input_file> <output_file>.mp4\n" Sys.argv.(0);
    exit 1);

  let src = Av.Input.open_url Sys.argv.(1) in
  
  let dst = Av.Output.open_file Sys.argv.(2) in
  
  let sid = Av.Output.new_audio_stream ~codec_id:Avcodec.Audio.AC_AAC
      ~codec_parameters:Av.(get_audio_codec_parameters@@of_input src) dst in

  Av.Input.iter_audio(fun frm -> Av.Output.write_audio_frame dst sid frm) src;

  Av.Input.close src;
  Av.Output.close dst;

  Gc.full_major ()
