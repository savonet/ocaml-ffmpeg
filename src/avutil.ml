module Pixel_format = struct
  type t =
  | YUV420p
  | YUYV422
  | RGB24
  | BGR24
  | YUV422p
  | YUV444p
  | YUV410p
  | YUV411p
  | YUVJ422p
  | YUVJ444p
  | RGBA
  | BGRA

  external bits : t -> int = "caml_avutil_bits_per_pixel"

  let bits (*?(padding=true)*) p =
    let n = bits p in
    (* TODO: when padding is true we should add the padding *)
    n
end

module Channel_layout = struct
  type t =
  | CL_mono
  | CL_stereo
  | CL_2point1
  | CL_2_1
  | CL_surround
  | CL_3point1
  | CL_4point0
  | CL_4point1
  | CL_2_2
  | CL_quad
  | CL_5point0
  | CL_5point1
  | CL_5point0_back
  | CL_5point1_back
  | CL_6point0
  | CL_6point0_front
  | CL_hexagonal
  | CL_6point1
  | CL_6point1_back
  | CL_6point1_front
  | CL_7point0
  | CL_7point0_front
  | CL_7point1
  | CL_7point1_wide
  | CL_7point1_wide_back
  | CL_octagonal
  | CL_hexadecagonal
  | CL_stereo_downmix
end

module Sample_format = struct
  type t =
  | SF_None
  | SF_Unsigned_8
  | SF_Signed_16
  | SF_Signed_32
  | SF_Float_32
  | SF_Float_64
  | SF_Unsigned_8_planar
  | SF_Signed_16_planar
  | SF_Signed_32_planar
  | SF_Float_32_planar
  | SF_Float_64_planar

let get_name = function
  | SF_None -> ""
  | SF_Unsigned_8 | SF_Unsigned_8_planar -> "u8"
  | SF_Signed_16 | SF_Signed_16_planar -> "s16le"
  | SF_Signed_32 | SF_Signed_32_planar -> "s32le"
  | SF_Float_32 | SF_Float_32_planar -> "f32le"
  | SF_Float_64 | SF_Float_64_planar -> "f64le"
end
