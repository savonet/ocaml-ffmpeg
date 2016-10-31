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


let () =
  let src = Array.init 55292 (fun t -> sin(float_of_int t *. 0.05)) in
  let dst = open_out_bin "test_swresample_out.raw" in

  let r0 = R0.create CL_mono 55292 CL_5point1 96000 in
  let r1 = R1.create CL_5point1 ~in_sample_format:SF_S16 96000 CL_stereo 16000 in
  let r2 = R2.create CL_stereo 16000 CL_surround 44100 in
  let r3 = R3.create CL_surround 44100 CL_stereo 48000 in
  let r4 = R4.create CL_stereo 48000 CL_hexadecagonal 31000 in
  let r5 = R5.create CL_hexadecagonal 31000 CL_stereo 73347 in
  let r6 = R6.create CL_stereo 73347 CL_stereo 44100 in
  let r7 = R7.create CL_stereo 44100 CL_stereo 44100 in
  let r8 = R8.create CL_stereo 44100 CL_stereo 96000 in

  src
  |> R0.convert r0
  |> R1.convert r1
  |> R2.convert r2
  |> R3.convert r3
  |> R4.convert r4
  |> R5.convert r5
  |> R6.convert r6
  |> R7.convert r7
  |> R8.convert r8
  |> output_bytes dst;

  close_out dst

