open FFmpeg
open Avcodec

let () =
  if Array.length Sys.argv < 5 then (
    Printf.eprintf "      usage: %s <input file> <input codec> <output file> <output codec>
      API example program to show how to read audio frames from an input file.
      This program parse data to packets from a file, decodes them, and writes decoded
      audio frames to a audio file named <output file>.\n" Sys.argv.(0);
    exit 1);

  Avutil.Log.set_level `Debug;
  Avutil.Log.set_callback print_string;
  
  let in_codec_id = Audio.find_id Sys.argv.(2) in

  let parser = Audio.create_parser in_codec_id in
  let decoder = Audio.create_decoder in_codec_id in

  let in_fd = Unix.openfile Sys.argv.(1) [Unix.O_RDONLY] 0 in

  let out_file = Av.open_output Sys.argv.(3) in
  let out_stream = Av.new_audio_stream ~codec_name:Sys.argv.(4) out_file in

  (* Unix.map_file in_fd Bigarray.Int8_unsigned Bigarray.c_layout false [|-1|] *)
  Bigarray.Genarray.map_file in_fd Bigarray.Int8_unsigned Bigarray.c_layout false [|-1|]
  |> Bigarray.array1_of_genarray
  |> Packet.parse_data parser @@ Avcodec.decode decoder @@ Av.write_frame out_stream;

  Avcodec.flush_decoder decoder @@ Av.write_frame out_stream;

  Unix.close in_fd;
  Av.close out_file;

  Gc.full_major (); Gc.full_major ();
