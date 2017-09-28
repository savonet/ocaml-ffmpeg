open FFmpeg
open Av
module If = Input.Format
module Of = Output.Format

let () =
  if Array.length Sys.argv < 2 then Printf.(
      printf "\ninput devices :";
      Avdevice.get_input_audio_devices() |> List.iter(fun d -> printf" %s"(If.get_name d));
      printf "\noutput devices :";
      Avdevice.get_output_audio_devices() |> List.iter(fun d -> printf" %s"(Of.get_name d));

      printf"\nusage: %s input [output]\ninput and output can be devices or file names\n" Sys.argv.(0);
      exit 1);

  let src = try Avdevice.get_input_audio_devices()
                |> List.find(fun d -> If.get_name d = Sys.argv.(1))
                |> Input.open_format
    with Not_found -> Input.open_url Sys.argv.(1) in

  let dst, codec_id = try Avdevice.get_output_audio_devices()
                          |> (if Array.length Sys.argv < 3 then List.hd
                              else List.find(fun d -> Of.get_name d = Sys.argv.(2)))
                          |> fun fmt ->
                          (Output.open_format fmt, Of.get_audio_codec fmt)
    with Not_found ->
      (Output.open_file Sys.argv.(2), Avcodec.Audio.AC_FLAC) in

  let sid = Output.new_audio_stream ~codec_id dst in

  let rec run n =
    if n > 0 then match Input.read_audio src with
      | Input.Audio frame -> Output.write_audio_frame dst sid frame;
        run(n - 1)
      | Input.End_of_file -> ()
  in
  run 100;

  Input.close src;
  Output.close dst;

  Gc.full_major ()
