open FFmpeg

let output_int32_bigarray dst ba =
  for i = 0 to Bigarray.Array1.dim ba - 1 do
    let v = Int32.(div ba.{i} (of_int 2) |> to_int) in
    output_binary_int dst v;
  done

let output_float_array dst ar =
  for i = 0 to Array.length ar - 1 do
    output_binary_int dst (int_of_float(ar.(i) *. 1_000_000_000.));
  done

let output_float32_planar_bigarray dst bas =
  for i = 0 to Bigarray.Array1.dim bas.(0) - 1 do
    output_binary_int dst (int_of_float(bas.(0).{i} *. 1_000_000_000.));
    output_binary_int dst (int_of_float(bas.(1).{i} *. 1_000_000_000.));
  done

let print_float32_planar_bigarray bas =
  for i = 0 to Bigarray.Array1.dim bas.(0) - 1 do
    let s = Bytes.of_string "                                        |                                        "
    in
    Bytes.set s (int_of_float(bas.(0).{i} *. 40.) + 40) 'R';
    Bytes.set s (int_of_float(bas.(1).{i} *. 40.) + 40) 'L';
    print_endline s
  done


let () =
  if Array.length Sys.argv < 3 then (
    Printf.eprintf {|
      usage: %s input_file audio_output_file
      API example program to show how to read audio frames from an input file. 
      This program reads best audio frames from a file, decodes them, and writes decoded
      audio frames to a rawaudio file named audio_output_file.

|}    Sys.argv.(0);
    exit 1
  );

  let audio_dst_filename = Sys.argv.(2) ^ ".raw" in
  let audio_dst_file = open_out_bin audio_dst_filename in
  let be1_dst_file = open_out_bin (Sys.argv.(2) ^ ".be1.raw") in
  let be2_dst_file = open_out_bin (Sys.argv.(2) ^ ".be2.raw") in
  let be3_dst_file = open_out_bin (Sys.argv.(2) ^ ".be3.raw") in
  let right_dst_file = open_out_bin (Sys.argv.(2) ^ ".right.raw") in
  let left_dst_file = open_out_bin (Sys.argv.(2) ^ ".left.raw") in

  let src_file = Av.open_input Sys.argv.(1) in
  Av.(set_audio_out_format ~channel_layout:CL_stereo
        ~sample_format:SF_Signed_32 ~sample_rate:44100 src_file);

  let rec decode() = 
    match Av.read_audio src_file with
    | af ->
      Av.audio_to_string af |> output_string audio_dst_file;
      Av.audio_to_float32_planar_bigarray af |> output_float32_planar_bigarray be1_dst_file;
      Av.audio_to_signed32_bigarray af |> output_int32_bigarray be2_dst_file;
      Av.audio_to_float_array af |> output_float_array be3_dst_file;
      let ar = Av.audio_to_planar_string af in
      output_string right_dst_file ar.(0);
      output_string left_dst_file ar.(1);
      decode()
    | exception Av.End_of_file -> ()
  in
  decode();
  Gc.full_major ();

  close_out audio_dst_file;
  close_out be1_dst_file;
  close_out be2_dst_file;
  close_out be3_dst_file;
  close_out right_dst_file;
  close_out left_dst_file;

  Printf.printf "Play the output audio file with the command:\nffplay -f %s -ac %d -ar %d %s\n"
    Av.(get_audio_out_sample_format src_file |> get_sample_fmt_name)
    Av.(get_audio_out_nb_channels src_file)
    Av.(get_audio_out_sample_rate src_file)
    audio_dst_filename

