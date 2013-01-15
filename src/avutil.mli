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

  val bits : ?padding:bool -> t -> int
end
