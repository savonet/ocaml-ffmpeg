open Avcodec

let () = Printexc.record_backtrace true

module Compat = struct
  let map_file _ _ _ _ _ = assert false
  let () = ignore map_file

  include Bigarray.Genarray
  include Unix
end

let () =
  if Array.length Sys.argv < 5 then (
    Printf.eprintf
      "      usage: %s <input file> <input codec> <output file> <output codec>\n\
      \      API example program to show how to read audio frames from an \
       input file.\n\
      \      This program parse data to packets from a file, decodes them, and \
       writes decoded\n\
      \      audio frames to a audio file named <output file>.\n"
      Sys.argv.(0);
    exit 1);

  Avutil.Log.set_level `Debug;
  Avutil.Log.set_callback print_string;

  let in_codec = Audio.find_decoder_by_name Sys.argv.(2) in

  let parser = Audio.create_parser in_codec in
  let decoder = Audio.create_decoder in_codec in

  let in_fd = Unix.openfile Sys.argv.(1) [Unix.O_RDONLY] 0 in

  let out_file = Av.open_output Sys.argv.(3) in
  let codec = Avcodec.Audio.find_encoder_by_name Sys.argv.(4) in
  let channel_layout = Avcodec.Audio.find_best_channel_layout codec `Stereo in
  let sample_format = Avcodec.Audio.find_best_sample_format codec `Dbl in
  let sample_rate = Avcodec.Audio.find_best_sample_rate codec 44100 in
  let time_base = { Avutil.num = 1; den = sample_rate } in
  let out_stream =
    Av.new_audio_stream ~channel_layout ~sample_format ~sample_rate ~time_base
      ~codec out_file
  in

  let filter = ref None in
  let get_filter frame =
    match !filter with
      | Some f -> f
      | None ->
          let in_params =
            {
              Avfilter.Utils.sample_rate =
                Avutil.Audio.frame_get_sample_rate frame;
              channel_layout = Avutil.Audio.frame_get_channel_layout frame;
              sample_format = Avutil.Audio.frame_get_sample_format frame;
            }
          in
          let in_time_base = { Avutil.num = 1; den = sample_rate } in
          let out_frame_size =
            if List.mem `Variable_frame_size (Avcodec.capabilities codec) then
              512
            else Av.get_frame_size out_stream
          in
          let out_params =
            { Avfilter.Utils.sample_rate; sample_format; channel_layout }
          in
          let f =
            Avfilter.Utils.init_audio_converter ~in_params ~in_time_base
              ~out_params ~out_frame_size ()
          in
          filter := Some f;
          f
  in

  let pts = ref 0L in
  let on_frame frame =
    Avutil.Frame.set_pts frame (Some !pts);
    pts := Int64.add !pts (Int64.of_int (Avutil.Audio.frame_nb_samples frame));
    Av.write_frame out_stream frame
  in

  let write_frame frame =
    let filter = get_filter frame in
    Avfilter.Utils.convert_audio filter on_frame frame
  in

  Compat.map_file in_fd Bigarray.Int8_unsigned Bigarray.c_layout false [| -1 |]
  |> Bigarray.array1_of_genarray
  |> Packet.parse_data parser @@ Avcodec.decode decoder @@ write_frame;

  Avcodec.flush_decoder decoder @@ write_frame;

  Unix.close in_fd;
  Av.close out_file;

  Gc.full_major ();
  Gc.full_major ()
