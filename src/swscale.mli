(** Video color conversion and scaling *)

val version : unit -> int

val configuration : unit -> string

val license : unit -> string

type pixel_format = Avutil.Pixel_format.t

type flag = Fast_bilinear | Bilinear | Bicubic | Print_info

type t

val create : flag list -> int -> int -> pixel_format -> int -> int -> pixel_format -> t
(** [Swscale.create flags in_w in_h in_pf out_w out_h out_pf] create a Swscale.t scale context with the [flags] flag list defining the conversion type, the [in_w] width, [in_h] height and [in_pf] pixel format for input format and [out_w] width, [out_h] height and [out_pf] pixel format for output format. *)
  
type data = (int, Bigarray.int8_unsigned_elt, Bigarray.c_layout) Bigarray.Array1.t

type planes = (data * int) array

val scale : t -> planes -> int -> int -> planes -> int -> unit
(** [Swscale.scale ctx in_planes y h out_planes off] scale the [h] rows of [in_planes] strarting from the row [y] to the [off] row of the [out_planes] output. *)
