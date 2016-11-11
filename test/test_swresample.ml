open FFmpeg
open Avutil
open Channel_layout
open Sample_format

module R0 = Swresample.Make (Swresample.FloatArray) (Swresample.S16Frame)
module R1 = Swresample.Make (Swresample.Frame) (Swresample.U8BigArray)
module R2 = Swresample.Make (Swresample.U8BigArray) (Swresample.DblPlanarFrame)
module R3 = Swresample.Make (Swresample.DblPlanarFrame) (Swresample.S32PlanarBigArray)
module R4 = Swresample.Make (Swresample.S32PlanarBigArray) (Swresample.FltPlanarBigArray)
module R5 = Swresample.Make (Swresample.FltPlanarBigArray) (Swresample.PlanarFloatArray)
module R6 = Swresample.Make (Swresample.PlanarFloatArray) (Swresample.S32Frame)
module R7 = Swresample.Make (Swresample.S32Frame) (Swresample.FloatArray)
module R8 = Swresample.Make (Swresample.FloatArray) (Swresample.S32Bytes)

let logStep step v = Printf.printf(step^^" done\n%!"); v

let foi = float_of_int
let pi = 4.0 *. atan 1.0
let rate = 10000
let frate = float_of_int rate
    
let () =
  let src = Array.init (20 * rate)
      (fun t -> sin((foi t *. pi *. 110. *. (2. ** (foi(4 * t / rate)))) /. frate))
  in
  logStep"Src"();
  let dst = open_out_bin "test_swresample_out.raw" in
  let r0 = R0.create CL_mono rate CL_5point1 96000 in
  let r1 = R1.create CL_5point1 ~in_sample_format:SF_S16 96000 CL_stereo 16000 in
  let r2 = R2.create CL_stereo 16000 CL_surround 44100 in
  let r3 = R3.create CL_surround 44100 CL_stereo 48000 in
  let r4 = R4.create CL_stereo 48000 CL_hexadecagonal 31000 in
  let r5 = R5.create CL_hexadecagonal 31000 CL_stereo 73347 in
  let r6 = R6.create CL_stereo 73347 CL_stereo 44100 in
  let r7 = R7.create CL_stereo 44100 CL_stereo 11025 in
  let r8 = R8.create CL_stereo 11025 CL_stereo 96000 in
  logStep"R.create"();

  src
  |> R0.convert r0 |> logStep "R0.convert"
  |> R1.convert r1 |> logStep "R1.convert"
  |> R2.convert r2 |> logStep "R2.convert"
  |> R3.convert r3 |> logStep "R3.convert"
  |> R4.convert r4 |> logStep "R4.convert"
  |> R5.convert r5 |> logStep "R5.convert"
  |> R6.convert r6 |> logStep "R6.convert"
  |> R7.convert r7 |> logStep "R7.convert"
  |> R8.convert r8 |> logStep "R8.convert"
  |> output_bytes dst |> logStep"output_bytes dst";

  close_out dst |> logStep"close_out dst"

