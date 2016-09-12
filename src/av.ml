exception End_of_file
exception Failure
exception Failure_msg of string
exception Open_failure of string

let () =
  Callback.register_exception "ffmpeg_exn_end_of_file" End_of_file;
  Callback.register_exception "ffmpeg_exn_failure" Failure;
  Callback.register_exception "ffmpeg_exn_failure_msg" (Failure_msg "");
  Callback.register_exception "ffmpeg_exn_open_failure" (Open_failure "")

type t

external open_input : string -> t = "ocaml_ffmpeg_open_input"

type channel_layout_t =
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

type sample_format_t =
  | SF_None
  | SF_Unsigned_8 | SF_Signed_16 | SF_Signed_32 | SF_Float_32 | SF_Float_64
  | SF_Unsigned_8_planar | SF_Signed_16_planar | SF_Signed_32_planar | SF_Float_32_planar | SF_Float_64_planar

let get_sample_fmt_name = function
  | SF_None -> ""
  | SF_Unsigned_8 | SF_Unsigned_8_planar -> "u8"
  | SF_Signed_16 | SF_Signed_16_planar -> "s16le"
  | SF_Signed_32 | SF_Signed_32_planar -> "s32le"
  | SF_Float_32 | SF_Float_32_planar -> "f32le"
  | SF_Float_64 | SF_Float_64_planar -> "f64le"


external set_audio_out_format : ?channel_layout:channel_layout_t -> ?sample_format:sample_format_t -> ?sample_rate:int -> t -> unit = "ocaml_ffmpeg_set_audio_out_format"

external get_audio_in_channel_layout : t -> channel_layout_t = "ocaml_ffmpeg_get_audio_in_channel_layout"
external get_audio_in_nb_channels : t -> int = "ocaml_ffmpeg_get_audio_in_nb_channels"
external get_audio_in_sample_rate : t -> int = "ocaml_ffmpeg_get_audio_in_sample_rate"
external get_audio_in_sample_format : t -> sample_format_t = "ocaml_ffmpeg_get_audio_in_sample_format"

external get_audio_out_channel_layout : t -> channel_layout_t = "ocaml_ffmpeg_get_audio_out_channel_layout"
external get_audio_out_nb_channels : t -> int = "ocaml_ffmpeg_get_audio_out_nb_channels"
external get_audio_out_sample_rate : t -> int = "ocaml_ffmpeg_get_audio_out_sample_rate"
external get_audio_out_sample_format : t -> sample_format_t = "ocaml_ffmpeg_get_audio_out_sample_format"

type audio_t
type video_t
type subtitle_t
type media_t =
    Audio of audio_t
  | Video of video_t
  | Subtitle of subtitle_t

external read_audio : t -> audio_t = "ocaml_ffmpeg_read_audio"
external read_video : t -> video_t = "ocaml_ffmpeg_read_video"
external read_subtitle : t -> subtitle_t = "ocaml_ffmpeg_read_subtitle"
external read : t -> media_t = "ocaml_ffmpeg_read"


type s16ba_t = (int, Bigarray.int16_signed_elt, Bigarray.c_layout) Bigarray.Array1.t
type s32ba_t = (int32, Bigarray.int32_elt, Bigarray.c_layout) Bigarray.Array1.t
type f32ba_t = (float, Bigarray.float32_elt, Bigarray.c_layout) Bigarray.Array1.t
type f64ba_t = (float, Bigarray.float64_elt, Bigarray.c_layout) Bigarray.Array1.t
type f32pba_t = f32ba_t array
type f64pba_t = f64ba_t array

external get_out_samples : ?sample_format:sample_format_t -> audio_t -> int = "ocaml_ffmpeg_get_out_samples"


external audio_to_string : audio_t -> string = "ocaml_ffmpeg_audio_to_string"
external audio_to_planar_string : audio_t -> string array = "ocaml_ffmpeg_audio_to_planar_string"
external audio_to_signed16_bigarray : audio_t -> s16ba_t = "ocaml_ffmpeg_audio_to_signed16_bigarray"
external audio_to_signed32_bigarray : audio_t -> s32ba_t = "ocaml_ffmpeg_audio_to_signed32_bigarray"
external audio_to_float64_bigarray : audio_t -> f64ba_t = "ocaml_ffmpeg_audio_to_float64_bigarray"
external video_to_string : video_t -> string = "ocaml_ffmpeg_video_to_string"
external subtitle_to_string : subtitle_t -> string = "ocaml_ffmpeg_subtitle_to_string"
(*
external to_float_array : audio_t -> float array = "ocaml_ffmpeg_to_float_array"
external to_float_planar_array : audio_t -> float array array = "ocaml_ffmpeg_to_float_planar_array"
external to_float32_bigarray : audio_t -> f32ba_t = "ocaml_ffmpeg_to_float32_bigarray"
external to_float32_planar_bigarray : audio_t -> f32pba_t = "ocaml_ffmpeg_to_float32_planar_bigarray"
external to_float64_planar_bigarray : audio_t -> f64pba_t = "ocaml_ffmpeg_to_float64_planar_bigarray"


external close_input : t -> unit = "ocaml_ffmpeg_close_input"
*)
