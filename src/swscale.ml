open Avutil

external version : unit -> int = "ocaml_swscale_version"

external configuration : unit -> string = "ocaml_swscale_configuration"

external license : unit -> string = "ocaml_swscale_configuration"

type pixel_format = Avutil.Pixel_format.t

type flag =
| Fast_bilinear
| Bilinear
| Bicubic
| Print_info

type t

external create : flag array -> int -> int -> pixel_format -> int -> int -> pixel_format -> t = "ocaml_swscale_get_context_byte" "ocaml_swscale_get_context"
let create flags = create (Array.of_list flags)

type data = (int, Bigarray.int8_unsigned_elt, Bigarray.c_layout) Bigarray.Array1.t

type planes = (data * int) array

external scale : t -> planes -> int -> int -> planes -> int -> unit = "ocaml_swscale_scale_byte" "ocaml_swscale_scale"


type vector_kind = Ba | Frm | Str

module type VideoData = sig type t val vk : vector_kind end

module BigArray = struct type t = planes let vk = Ba end

module Frame = struct type t = video frame let vk = Frm end

module Bytes = struct type t = (string * int) array let vk = Str end

type ('i, 'o) ctx

module Make (I : VideoData) (O : VideoData) = struct
  type t = (I.t, O.t) ctx

  external create : flag array -> vector_kind -> int -> int -> pixel_format -> vector_kind -> int -> int -> pixel_format ->
    t = "ocaml_swscale_create_byte" "ocaml_swscale_create"

  let create flags in_width in_height in_pixel_format
      out_width out_height out_pixel_format =

    create (Array.of_list flags) I.vk in_width in_height in_pixel_format
      O.vk out_width out_height out_pixel_format

(*  
  let from_codec flags in_codec out_width out_height out_pixel_format =

    create flags
      (Avcodec.Video.get_width in_codec)
      (Avcodec.Video.get_height in_codec)
      (Avcodec.Video.get_pixel_format in_codec)
      out_width out_height out_pixel_format


  let to_codec flags in_width in_height in_pixel_format out_codec =

    create flags
      in_width in_height in_pixel_format
      (Avcodec.Video.get_width out_codec)
      (Avcodec.Video.get_height out_codec)
      (Avcodec.Video.get_pixel_format out_codec)


  let from_codec_to_codec flags in_codec out_codec =

    create flags
      (Avcodec.Video.get_width in_codec)
      (Avcodec.Video.get_height in_codec)
      (Avcodec.Video.get_pixel_format in_codec)
      (Avcodec.Video.get_width out_codec)
      (Avcodec.Video.get_height out_codec)
      (Avcodec.Video.get_pixel_format out_codec)
*)

  external reuse_output : t -> bool -> unit = "ocaml_swscale_reuse_output"

  external convert : t -> I.t -> O.t = "ocaml_swscale_convert"
end
