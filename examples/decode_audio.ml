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
    exit 1 );

  Avutil.Log.set_level `Debug;
  Avutil.Log.set_callback print_string;

  let in_codec = Audio.find_decoder Sys.argv.(2) in

  let parser = Audio.create_parser in_codec in
  let decoder = Audio.create_decoder in_codec in

  let in_fd = Unix.openfile Sys.argv.(1) [Unix.O_RDONLY] 0 in

  let out_file = Av.open_output Sys.argv.(3) in
  let out_codec = Avcodec.Audio.find_encoder Sys.argv.(4) in
  let out_stream = Av.new_audio_stream ~codec:out_codec out_file in

  let frame_size =
    if List.mem `Variable_frame_size (Audio.capabilities out_codec) then 512
    else Av.get_frame_size out_stream
  in

  let filters = ref None in
  let get_filters frame =
    match !filters with
      | Some f -> f
      | None ->
          let sample_rate = Avutil.Audio.frame_get_sample_rate frame in
          let time_base = { Avutil.num = 1; den = sample_rate } in
          let channels = Avutil.Audio.frame_get_channels frame in
          let channel_layout = Avutil.Audio.frame_get_channel_layout frame in
          let sample_format = Avutil.Audio.frame_get_sample_format frame in
          let f =
            Avfilter.Utils.split_frame_size ~sample_rate ~time_base ~channels
              ~channel_layout ~sample_format frame_size
          in
          filters := Some f;
          f
  in

  let pts = ref 0L in
  let rec flush filter_out =
    try
      filter_out () |> fun frame ->
      Avutil.frame_set_pts frame (Some !pts);
      pts := Int64.add !pts (Int64.of_int (Avutil.Audio.frame_nb_samples frame));
      Av.write_frame out_stream frame;
      flush filter_out
    with Avutil.Error `Eagain -> ()
  in

  let write_frame frame =
    let filter_in, filter_out = get_filters frame in
    filter_in frame;
    flush filter_out
  in

  Compat.map_file in_fd Bigarray.Int8_unsigned Bigarray.c_layout false [| -1 |]
  |> Bigarray.array1_of_genarray
  |> Packet.parse_data parser @@ Avcodec.decode decoder @@ write_frame;

  Avcodec.flush_decoder decoder @@ write_frame;

  Unix.close in_fd;
  Av.close out_file;

  Gc.full_major ();
  Gc.full_major ()
