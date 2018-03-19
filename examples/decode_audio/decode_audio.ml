open FFmpeg
open Avcodec

let () =
  if Array.length Sys.argv < 5 then (
    Printf.eprintf "      usage: %s <input file> <input codec> <output file> <output codec>
      API example program to show how to read audio frames from an input file.
      This program parse data to packets from a file, decodes them, and writes decoded
      audio frames to a audio file named <output file>.\n" Sys.argv.(0);
    exit 1);

  let in_codec_id = Audio.find_id Sys.argv.(2) in

  let decoder = Audio.create_decoder in_codec_id in
  let parser = Audio.create_parser in_codec_id in

  let in_file = open_in_bin Sys.argv.(1) in
  let in_buf = Bytes.create 20480 in

  let out_codec_id = Audio.find_id Sys.argv.(4) in

  let out_file = Av.open_output Sys.argv.(3) in
  let out_stream = Av.new_audio_stream ~codec_id:out_codec_id out_file in

  let rec read_audio() =
    let len = input in_file in_buf 0 (Bytes.length in_buf) in

    if len > 0 then
      Packet.parse_bytes parser in_buf len
      |> Array.iter(fun pkt ->
          Avcodec.decode decoder pkt
          |> Array.iter(Av.write out_stream))
    else ()
  in
  read_audio();

  Avcodec.flush_decoder decoder |> Array.iter(Av.write out_stream);

  close_in in_file;
  Av.close out_file;

  Gc.full_major (); Gc.full_major ();
