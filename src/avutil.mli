module Pixel_format : sig
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

  val bits : (*?padding:bool ->*) t -> int
end

module Channel_layout : sig
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

module Sample_format : sig
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

  val get_name : t -> string
end

module Audio_frame : sig
  type t
end

module Video_frame : sig
  type t
end

module Subtitle_frame : sig
  type t
end
