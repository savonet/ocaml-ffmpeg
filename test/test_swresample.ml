open FFmpeg
open Avutil
open Channel_layout
open Sample_format

module R = Swresample.Make (Swresample.FloatArray) (Swresample.S32Bytes)
module R0 = Swresample.Make (Swresample.FloatArray) (Swresample.S16Frame)
module R1 = Swresample.Make (Swresample.Frame) (Swresample.U8BigArray)
module R2 = Swresample.Make (Swresample.U8BigArray) (Swresample.DblPlanarFrame)
module R3 = Swresample.Make (Swresample.DblPlanarFrame) (Swresample.S32PlanarBigArray)
module R4 = Swresample.Make (Swresample.S32PlanarBigArray) (Swresample.FltPlanarBigArray)
module R5 = Swresample.Make (Swresample.FltPlanarBigArray) (Swresample.PlanarFloatArray)
module R6 = Swresample.Make (Swresample.PlanarFloatArray) (Swresample.S32Frame)
module R7 = Swresample.Make (Swresample.S32Frame) (Swresample.FloatArray)
module R8 = Swresample.Make (Swresample.FloatArray) (Swresample.S32BigArray)
module R9 = Swresample.Make (Swresample.S32BigArray) (Swresample.S32Bytes)

let logStep step v = Gc.full_major (); Printf.printf"%s done\n%!" step; v

let foi = float_of_int
let pi = 4.0 *. atan 1.0
let rate = 44100
let frate = float_of_int rate

let () =
  let dst1 = open_out_bin "test_swresample_out1.raw" in
  let r = R.create CL_mono rate CL_stereo 44100 in

  let dst2 = open_out_bin "test_swresample_out2.raw" in
  let r0 = R0.create CL_mono rate CL_5point1 96000 in
  let r1 = R1.create CL_5point1 ~in_sample_format:SF_S16 96000 CL_stereo 16000 in
  let r2 = R2.create CL_stereo 16000 CL_surround 44100 in
  let r3 = R3.create CL_surround 44100 CL_stereo 48000 in
  let r4 = R4.create CL_stereo 48000 CL_hexadecagonal 31000 in
  let r5 = R5.create CL_hexadecagonal 31000 CL_stereo 73347 in
  let r6 = R6.create CL_stereo 73347 CL_stereo 44100 in
  let r7 = R7.create CL_stereo 44100 CL_stereo 48000 in
  let r8 = R8.create CL_stereo 48000 CL_stereo 96000 in
  let r9 = R9.create CL_stereo 96000 CL_stereo 44100 in
  logStep"R.create"();

  for note = 0 to 95 do

    let freq = 55. *. (2. ** ((foi note) /. 12.)) in
    let len = int_of_float((frate /. freq) *. (floor(freq /. 4.))) in
    let c = (2. *. pi *. freq) /. frate in
    let src = Array.init len (fun t -> sin(foi t *. c)) |> logStep"src" in

    src |> R.convert r |> logStep"convert r" |> output_bytes dst1 |> logStep"output_bytes dst1";

    src
    |> R0.convert r0 |> logStep"convert r0"
    |> R1.convert r1 |> logStep"convert r1"
    |> R2.convert r2 |> logStep"convert r2"
    |> R3.convert r3 |> logStep"convert r3"
    |> R4.convert r4 |> logStep"convert r4"
    |> R5.convert r5 |> logStep"convert r5"
    |> R6.convert r6 |> logStep"convert r6"
    |> R7.convert r7 |> logStep"convert r7"
    |> R8.convert r8 |> logStep"convert r8"
    |> R9.convert r9 |> logStep"convert r9"
    |> output_bytes dst2 |> logStep"output_bytes dst2"
    |> logStep("note " ^ string_of_int note)
  done;

  close_out dst1 |> logStep"close_out dst1";
  close_out dst2 |> logStep"close_out dst2";


