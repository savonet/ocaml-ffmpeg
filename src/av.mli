exception End_of_file
exception Failure
exception Failure_msg of string
exception Open_failure of string
type t
val open_input : string -> t
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
    None
  | Unsigned_8
  | Signed_16
  | Signed_32
  | Float_32
  | Float_64
  | Unsigned_8_planar
  | Signed_16_planar
  | Signed_32_planar
  | Float_32_planar
  | Float_64_planar

val set_audio_out_format : ?channel_layout:channel_layout_t -> ?sample_rate:int -> t -> unit

val get_audio_in_channel_layout : t -> channel_layout_t
val get_audio_in_nb_channels : t -> int
val get_audio_in_sample_rate : t -> int
val get_audio_in_sample_format : t -> sample_format_t

val get_audio_out_channel_layout : t -> channel_layout_t
val get_audio_out_nb_channels : t -> int
val get_audio_out_sample_rate : t -> int
val get_audio_out_sample_format : t -> sample_format_t

type audio_frame_t
type video_frame_t
type subtitle_frame_t
type frame_t =
    AudioFrame of audio_frame_t
  | VideoFrame of video_frame_t
  | SubtitleFrame of subtitle_frame_t

val read_audio_frame : t -> audio_frame_t
val read_video_frame : t -> video_frame_t

type f32ba_t =
    (float, Bigarray.float32_elt, Bigarray.c_layout) Bigarray.Array1.t
type f64ba_t =
    (float, Bigarray.float64_elt, Bigarray.c_layout) Bigarray.Array1.t
type f32pba_t = f32ba_t array
type f64pba_t = f64ba_t array
val get_out_samples :
  ?sample_format:sample_format_t -> audio_frame_t -> int
val to_string : audio_frame_t -> string
val to_float64_bigarray : audio_frame_t -> f64ba_t
