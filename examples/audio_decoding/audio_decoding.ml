open FFmpeg
open Avutil

let output_int32_ba dst ba =
  for i = 0 to Bigarray.Array1.dim ba - 1 do
    let v = Int32.(div ba.{i} (of_int 2) |> to_int) in
    output_binary_int dst v;
  done

let output_float_array dst ar =
  for i = 0 to Array.length ar - 1 do
    output_binary_int dst (int_of_float(ar.(i) *. 1_000_000_000.));
  done

let output_float32_planar_ba dst bas =
  for i = 0 to Bigarray.Array1.dim bas.(0) - 1 do
    output_binary_int dst (int_of_float(bas.(0).{i} *. 1_000_000_000.));
    output_binary_int dst (int_of_float(bas.(1).{i} *. 1_000_000_000.));
  done

let print_float32_planar_ba bas =
  for i = 0 to Bigarray.Array1.dim bas.(0) - 1 do
    let s = Bytes.of_string "                                        |                                        "
    in
    Bytes.set s (int_of_float(bas.(0).{i} *. 40.) + 40) 'R';
    Bytes.set s (int_of_float(bas.(1).{i} *. 40.) + 40) 'L';
    print_endline s
  done

let format_parameter fmt =
  Sample_format.get_name fmt ^ if Sys.big_endian then "be" else "le"


module R = Swresample.(Converter(FRM) (S32_BA))


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
  let audio_ofd = open_out_bin audio_dst_filename in
  let be1_ofd = open_out_bin (Sys.argv.(2) ^ ".be1.raw") in
  let be2_ofd = open_out_bin (Sys.argv.(2) ^ ".be2.raw") in
  let be3_ofd = open_out_bin (Sys.argv.(2) ^ ".be3.raw") in
  let right_ofd = open_out_bin (Sys.argv.(2) ^ ".right.raw") in
  let left_ofd = open_out_bin (Sys.argv.(2) ^ ".left.raw") in
  let be_right_ofd = open_out_bin (Sys.argv.(2) ^ ".be.right.raw") in
  let be_left_ofd = open_out_bin (Sys.argv.(2) ^ ".be.left.raw") in

  let src = Av.open_input Sys.argv.(1) in

  let r = R.create (Av.get_channel_layout src) ~in_sample_format:(Av.get_sample_format src) (Av.get_sample_rate src)
      Channel_layout.CL_stereo 44100
  in
  (*
  let rs = Av.create_resample ~channel_layout:Channel_layout.CL_stereo
      ~sample_format:Sample_format.SF_S32 ~sample_rate:44100 src
  in
  *)
  let rec decode() = 
    match Av.read_audio src with
    | af ->(*
      Swresample.frame_to_float32_planar_ba rs af |> output_float32_planar_ba be1_ofd;
      Swresample.frame_to_int32_ba rs af |> output_int32_ba be2_ofd;
*)
      R.convert r af |> output_int32_ba be2_ofd;
      (*
      Swresample.frame_to_float_array rs af |> output_float_array be3_ofd;
      let str_ar = Swresample.frame_to_planar_string rs af in
      output_string right_ofd str_ar.(0);
      output_string left_ofd str_ar.(1);
      let ar_ar = Swresample.frame_to_float_planar_array rs af in
      output_float_array be_right_ofd ar_ar.(0);
      output_float_array be_left_ofd ar_ar.(1);
      Swresample.frame_to_string rs af |> output_string audio_ofd;
*)
      decode()
    | exception Av.End_of_file -> () (*R.flush r |> output_int32_ba be2_ofd*)
  in
  decode();
  Gc.full_major ();

  close_out audio_ofd;
  close_out be1_ofd;
  close_out be2_ofd;
  close_out be3_ofd;
  close_out right_ofd;
  close_out left_ofd;
  close_out be_right_ofd;
  close_out be_left_ofd;

  Printf.printf "Play the output audio file with the command:\nffplay -f %s -ac %d -ar %d %s\n"
    Av.(get_sample_format src |> format_parameter)
    Av.(get_nb_channels src)
    Av.(get_sample_rate src)
    audio_dst_filename

