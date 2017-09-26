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


