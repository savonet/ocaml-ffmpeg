open FFmpeg
module If = Av.Input_format
module Of = Av.Output_format
module Resampler = Swresample.Make (Swresample.Frame) (Swresample.Frame)

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
                |> Av.open_input_format
    with Not_found -> Av.open_input Sys.argv.(1) in

  let dst, codec_id = try Avdevice.get_output_audio_devices()
                          |> (if Array.length Sys.argv < 3 then List.hd
                              else List.find(fun d -> Of.get_name d = Sys.argv.(2)))
                          |> fun fmt ->
                          (Av.open_output_format fmt, Of.get_audio_codec fmt)
    with Not_found ->
      (Av.open_output Sys.argv.(2), Avcodec.Audio.AC_FLAC) in

  let sid = Av.new_audio_stream ~codec_id dst in

  let rsp = Resampler.from_input_to_output src dst in

  let rec run n =
    if n > 0 then match Av.read_audio src with
      | Av.Audio frm -> Resampler.convert rsp frm |> Av.write_audio_frame dst sid;
        run(n - 1)
      | Av.End_of_file -> ()
  in
  run 1000;

  Av.close_input src;
  Av.close_output dst;

  Gc.full_major ()
