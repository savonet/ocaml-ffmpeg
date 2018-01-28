open Avutil

type 'a t
type 'a context
type data = (int, Bigarray.int8_unsigned_elt, Bigarray.c_layout) Bigarray.Array1.t

(** Audio codecs. *)
module Audio = struct
  (** Audio codec ids *)
  type id = Codec_id.audio

  external get_name : id -> string = "ocaml_avcodec_get_audio_codec_name"

  external find_id : string -> id = "ocaml_avcodec_find_audio_codec_id"

  external find_best_sample_format : id -> Avutil.Sample_format.t = "ocaml_avcodec_find_best_sample_format"

  external get_id : audio t -> id = "ocaml_avcodec_parameters_get_audio_codec_id"
  external get_channel_layout : audio t -> Avutil.Channel_layout.t = "ocaml_avcodec_parameters_get_channel_layout"
  external get_nb_channels : audio t -> int = "ocaml_avcodec_parameters_get_nb_channels"
  external get_sample_format : audio t -> Avutil.Sample_format.t = "ocaml_avcodec_parameters_get_sample_format"
  external get_bit_rate : audio t -> int = "ocaml_avcodec_parameters_get_bit_rate"
  external get_sample_rate : audio t -> int = "ocaml_avcodec_parameters_get_sample_rate"

  external create_context : id -> Avutil.Channel_layout.t -> Avutil.Sample_format.t -> ?bit_rate:int -> int -> audio context = "ocaml_avcodec_audio_create_context"

  external decode : audio context -> data -> int -> audio frame array = "ocaml_avcodec_decode"

  external encode : audio context -> audio frame -> data = "ocaml_avcodec_encode"
end

(** Video codecs. *)
module Video = struct
  (** Video codec ids *)
  type id = Codec_id.video

  external get_name : id -> string = "ocaml_avcodec_get_video_codec_name"

  external find_id : string -> id = "ocaml_avcodec_find_video_codec_id"

  external find_best_pixel_format : id -> Avutil.Pixel_format.t = "ocaml_avcodec_find_best_pixel_format"

  external get_id : video t -> id = "ocaml_avcodec_parameters_get_video_codec_id"
  external get_width : video t -> int = "ocaml_avcodec_parameters_get_width"
  external get_height : video t -> int = "ocaml_avcodec_parameters_get_height"
  external get_sample_aspect_ratio : video t -> int * int = "ocaml_avcodec_parameters_get_sample_aspect_ratio"
  external get_pixel_format : video t -> Avutil.Pixel_format.t = "ocaml_avcodec_parameters_get_pixel_format"
  external get_bit_rate : video t -> int = "ocaml_avcodec_parameters_get_bit_rate"

  external create_context : id -> int -> int -> Avutil.Pixel_format.t -> video context = "ocaml_avcodec_video_create_context"

  external decode : video context -> data -> int -> video frame array = "ocaml_avcodec_decode"

  external encode : video context -> video frame -> data = "ocaml_avcodec_encode"
end

(** Subtitle codecs. *)
module Subtitle = struct
  (** Subtitle codec ids *)
  type id = Codec_id.subtitle

  external get_name : id -> string = "ocaml_avcodec_get_subtitle_codec_name"

  external find_id : string -> id = "ocaml_avcodec_find_subtitle_codec_id"

  external get_id : subtitle t -> id = "ocaml_avcodec_parameters_get_subtitle_codec_id"
end
