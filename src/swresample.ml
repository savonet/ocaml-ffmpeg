external version : unit -> int = "ocaml_swresample_version"

external configuration : unit -> string = "ocaml_swresample_configuration"

external license : unit -> string = "ocaml_swresample_configuration"

type pixel_format = Avutil.Pixel_format.t

type channel_layout = Avutil.Channel_layout.t

type sample_format = Avutil.Sample_format.t

type t


external create : channel_layout -> sample_format -> int -> channel_layout -> sample_format -> int -> t = "ocaml_swresample_get_context_byte" "ocaml_swresample_get_context"


type data = (int, Bigarray.int8_unsigned_elt, Bigarray.c_layout) Bigarray.Array1.t

type planes = (data * int) array

external convert : t -> planes -> int -> int -> planes -> int -> unit = "ocaml_swresample_convert_byte" "ocaml_swresample_convert"
