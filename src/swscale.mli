val version : unit -> int

val configuration : unit -> string

val license : unit -> string

type pixel_format = Avutil.Pixel_format.t

type flag = Fast_bilinear | Bilinear | Bicubic | Print_info

type t

val create : flag list -> int -> int -> pixel_format -> int -> int -> pixel_format -> t

type data = (int, Bigarray.int8_unsigned_elt, Bigarray.c_layout) Bigarray.Array1.t

type planes = (data * int) array

val scale : t -> planes -> int -> int -> planes -> int -> unit
