module R = Swresample.Make (Swresample.FloatArray) (Swresample.S32Bytes)
module R0 = Swresample.Make (Swresample.FloatArray) (Swresample.S16Frame)
module R1 = Swresample.Make (Swresample.Frame) (Swresample.U8BigArray)
module R2 = Swresample.Make (Swresample.U8BigArray) (Swresample.DblPlanarFrame)

module R3 =
  Swresample.Make (Swresample.DblPlanarFrame) (Swresample.S32PlanarBigArray)

module R4 =
  Swresample.Make (Swresample.S32PlanarBigArray) (Swresample.FltPlanarBigArray)

module R5 =
  Swresample.Make (Swresample.FltPlanarBigArray) (Swresample.PlanarFloatArray)

module R6 = Swresample.Make (Swresample.PlanarFloatArray) (Swresample.S32Frame)
module R7 = Swresample.Make (Swresample.S32Frame) (Swresample.FloatArray)
module R8 = Swresample.Make (Swresample.FloatArray) (Swresample.S32BigArray)
module R9 = Swresample.Make (Swresample.S32BigArray) (Swresample.S32Bytes)
module R10 = Swresample.Make (Swresample.S32Bytes) (Swresample.S32Bytes)
module ConverterInput = Swresample.Make (Swresample.Frame)
module Converter = ConverterInput (Swresample.PlanarFloatArray)

let logStep step v =
  Gc.full_major ();
  Printf.printf"%s done\n%!" step;
  v

let write_bytes = output_bytes

(* let write_bytes dst bytes = () *)

let foi = float_of_int
let pi = 4.0 *. atan 1.0
let rate = 44100
let frate = float_of_int rate

let test () =
  let dst1 = open_out_bin "test_swresample_out1.raw" in
  let r = R.create `Mono rate `Stereo 44100 in

  let dst2 = open_out_bin "test_swresample_out2.raw" in

  let r0 = R0.create `Mono rate `_5point1 96000 in
  let r1 = R1.create `_5point1 ~in_sample_format:`S16 96000 `Stereo 16000 in
  let r2 = R2.create `Stereo 16000 `Surround 44100 in
  let r3 = R3.create `Surround 44100 `Stereo 48000 in
  let r4 = R4.create `Stereo 48000 `Stereo_downmix 31000 in
  let r5 = R5.create `Stereo_downmix 31000 `Stereo 73347 in
  let r6 = R6.create `Stereo 73347 `Stereo 44100 in
  let r7 = R7.create `Stereo 44100 `Stereo 48000 in
  let r8 = R8.create `Stereo 48000 `Stereo 96000 in
  let r9 = R9.create `Stereo 96000 `Stereo 44100 in
  let r10 = R10.create `Stereo 44100 `Mono 44100 in

  R0.reuse_output r0 true;
  R4.reuse_output r4 true;
  R10.reuse_output r10 true;

  logStep "R.create" ();

  for note = 0 to 95 do
    let freq = 22.5 *. (2. ** (foi note /. 12.)) in
    let len = int_of_float (frate /. freq *. floor (freq /. 4.)) in
    let c = 2. *. pi *. freq /. frate in
    let src =
      Array.init len (fun t -> sin (foi t *. c))
      |> logStep ("src of len " ^ string_of_int len)
    in

    src |> R.convert r |> logStep "convert r" |> write_bytes dst1
    |> logStep "write_bytes dst1";

    src |> R0.convert r0 |> logStep "convert r0" |> R1.convert r1
    |> logStep "convert r1" |> R2.convert r2 |> logStep "convert r2"
    |> R3.convert r3 |> logStep "convert r3" |> R4.convert r4
    |> logStep "convert r4" |> R5.convert r5 |> logStep "convert r5"
    |> R6.convert r6 |> logStep "convert r6" |> R7.convert r7
    |> logStep "convert r7" |> R8.convert r8 |> logStep "convert r8"
    |> R9.convert r9 |> logStep "convert r9" |> R10.convert r10
    |> logStep "convert r10" |> write_bytes dst2 |> logStep "write_bytes dst2";

    logStep ("note " ^ string_of_int note) ()
  done;

  close_out dst1 |> logStep "close_out dst1";
  close_out dst2 |> logStep "close_out dst2";

  let output_planar_float_to_s16le audio_output_file planes =
    let nb_chan = Array.length planes in
    let bytes = Bytes.create 2 in
    let cx = float_of_int 0x7FFF in

    for i = 0 to Array.length planes.(0) - 1 do
      for c = 0 to nb_chan - 1 do
        let v = int_of_float (planes.(c).(i) *. cx) in

        Bytes.set bytes 0 (char_of_int (v land 0xFF));
        Bytes.set bytes 1 (char_of_int ((v lsr 8) land 0xFF));
        write_bytes audio_output_file bytes
      done
    done
  in

  Sys.argv |> Array.to_list |> List.tl
  |> List.iter (fun url ->
         try
           let idx, is, ic = Av.open_input url |> Av.find_best_audio_stream in
           let rsp = Converter.from_codec ic `Stereo 44100 in

           let p = try String.rindex url '/' + 1 with Not_found -> 0 in
           let audio_output_filename =
             String.(
               sub url p (length url - p)
               ^ "." ^ string_of_int idx ^ ".s16le.raw")
           in
           let audio_output_file = open_out_bin audio_output_filename in

           print_endline ("Convert " ^ url ^ " to " ^ audio_output_filename);

           is
           |> Av.iter_frame (fun frame ->
                  Converter.convert rsp frame
                  |> output_planar_float_to_s16le audio_output_file);

           Av.get_input is |> Av.close;
           close_out audio_output_file
         with _ -> print_endline ("No audio stream in " ^ url));

  Gc.full_major ();
  Gc.full_major ()
