exception End_of_file
exception Failure
exception Failure_msg of string
exception Open_failure of string
type t
val open_input : string -> t
val get_metadata : t -> (string * string) list

type channel_layout_t =
    CL_mono
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
type sample_format_t =
    SF_None
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

val get_sample_fmt_name : sample_format_t -> string

val set_audio_out_format : ?channel_layout:channel_layout_t -> ?sample_format:sample_format_t -> ?sample_rate:int -> t -> unit

val get_audio_in_channel_layout : t -> channel_layout_t
val get_audio_in_nb_channels : t -> int
val get_audio_in_sample_rate : t -> int
val get_audio_in_sample_format : t -> sample_format_t

val get_audio_out_channel_layout : t -> channel_layout_t
val get_audio_out_nb_channels : t -> int
val get_audio_out_sample_rate : t -> int
val get_audio_out_sample_format : t -> sample_format_t

type audio_t
type video_t
type subtitle_t
type media_t =
    Audio of audio_t
  | Video of video_t
  | Subtitle of subtitle_t

val read_audio : t -> audio_t
val read_video : t -> video_t
val read_subtitle : t -> subtitle_t
val read : t -> media_t

type u8ba_t = (int, Bigarray.int8_unsigned_elt, Bigarray.c_layout) Bigarray.Array1.t
type s16ba_t = (int, Bigarray.int16_signed_elt, Bigarray.c_layout) Bigarray.Array1.t
type s32ba_t = (int32, Bigarray.int32_elt, Bigarray.c_layout) Bigarray.Array1.t
type f32ba_t = (float, Bigarray.float32_elt, Bigarray.c_layout) Bigarray.Array1.t
type f64ba_t = (float, Bigarray.float64_elt, Bigarray.c_layout) Bigarray.Array1.t
type u8pba_t = u8ba_t array
type s16pba_t = s16ba_t array
type s32pba_t = s32ba_t array
type f32pba_t = f32ba_t array
type f64pba_t = f64ba_t array

val get_out_samples :
  ?sample_format:sample_format_t -> audio_t -> int

val audio_to_string : audio_t -> string
val audio_to_planar_string : audio_t -> string array

val audio_to_float_array : audio_t -> float array
val audio_to_float_planar_array : audio_t -> float array array

val audio_to_unsigned8_bigarray : audio_t -> u8ba_t
val audio_to_signed16_bigarray : audio_t -> s16ba_t
val audio_to_signed32_bigarray : audio_t -> s32ba_t
val audio_to_float32_bigarray : audio_t -> f32ba_t
val audio_to_float64_bigarray : audio_t -> f64ba_t

val audio_to_unsigned8_planar_bigarray : audio_t -> u8pba_t
val audio_to_signed16_planar_bigarray : audio_t -> s16pba_t
val audio_to_signed32_planar_bigarray : audio_t -> s32pba_t
val audio_to_float32_planar_bigarray : audio_t -> f32pba_t
val audio_to_float64_planar_bigarray : audio_t -> f64pba_t

val video_to_string : video_t -> string
val subtitle_to_string : subtitle_t -> string
