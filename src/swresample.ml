external version : unit -> int = "ocaml_swresample_version"

external configuration : unit -> string = "ocaml_swresample_configuration"

external license : unit -> string = "ocaml_swresample_configuration"

type pixel_format = Avutil.Pixel_format.t

type channel_layout = Avutil.Channel_layout.t

type sample_format = Avutil.Sample_format.t

type audio_frame = Avutil.Audio_frame.t

type t


external create : channel_layout -> sample_format -> int -> channel_layout -> sample_format -> int -> t = "ocaml_swresample_create_byte" "ocaml_swresample_create"


type u8ba = (int, Bigarray.int8_unsigned_elt, Bigarray.c_layout) Bigarray.Array1.t
type s16ba = (int, Bigarray.int16_signed_elt, Bigarray.c_layout) Bigarray.Array1.t
type s32ba = (int32, Bigarray.int32_elt, Bigarray.c_layout) Bigarray.Array1.t
type f32ba = (float, Bigarray.float32_elt, Bigarray.c_layout) Bigarray.Array1.t
type f64ba = (float, Bigarray.float64_elt, Bigarray.c_layout) Bigarray.Array1.t
type u8pba = u8ba array
type s16pba = s16ba array
type s32pba = s32ba array
type f32pba = f32ba array
type f64pba = f64ba array

external to_string : t -> audio_frame -> string = "ocaml_swresample_to_string"
external to_planar_string : t -> audio_frame -> string array = "ocaml_swresample_to_planar_string"

external to_float_array : t -> audio_frame -> float array = "ocaml_swresample_to_float_array"
external to_float_planar_array : t -> audio_frame -> float array array = "ocaml_swresample_to_float_planar_array"

external to_uint8_ba : t -> audio_frame -> u8ba = "ocaml_swresample_to_uint8_ba"
external to_int16_ba : t -> audio_frame -> s16ba = "ocaml_swresample_to_int16_ba"
external to_int32_ba : t -> audio_frame -> s32ba = "ocaml_swresample_to_int32_ba"
external to_float32_ba : t -> audio_frame -> f32ba = "ocaml_swresample_to_float32_ba"
external to_float64_ba : t -> audio_frame -> f64ba = "ocaml_swresample_to_float64_ba"

external to_uint8_planar_ba : t -> audio_frame -> u8pba = "ocaml_swresample_to_uint8_planar_ba"
external to_int16_planar_ba : t -> audio_frame -> s16pba = "ocaml_swresample_to_int16_planar_ba"
external to_int32_planar_ba : t -> audio_frame -> s32pba = "ocaml_swresample_to_int32_planar_ba"
external to_float32_planar_ba : t -> audio_frame -> f32pba = "ocaml_swresample_to_float32_planar_ba"
external to_float64_planar_ba : t -> audio_frame -> f64pba = "ocaml_swresample_to_float64_planar_ba"

