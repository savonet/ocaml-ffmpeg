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

  let bits ?(padding=true) p =
    let n = bits p in
    (* TODO: when padding is true we should add the padding *)
    n
end
