open Avutil

type 'a t
type ('line, 'media) context
type use = [ `decoder | `encoder]

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

  
  external create_context : id -> bool -> int option -> int option -> (_, audio)context = "ocaml_avcodec_create_context"

  let create_decoder_context id = create_context id true None None
  let create_encoder_context ?bit_rate id = create_context id false bit_rate None

  external decode : (input, audio)context -> bytes -> int -> audio frame array = "ocaml_avcodec_decode"
  external decode_data : (input, audio)context -> data -> int -> audio frame array = "ocaml_avcodec_decode_data"

  external _encode : (output, audio)context -> audio frame option -> bytes = "ocaml_avcodec_encode"
  let encode ctx frame = _encode ctx (Some frame)
  let flush ctx = _encode ctx None

  external _encode_to_data : (output, audio)context -> audio frame option -> data = "ocaml_avcodec_encode_to_data"
  let encode_to_data ctx frame = _encode_to_data ctx (Some frame)
  let flush_to_data ctx = _encode_to_data ctx None
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

  
  external create_context : id -> bool -> int option -> int option -> (_, video)context = "ocaml_avcodec_create_context"

  let create_decoder_context id = create_context id true None None
  let create_encoder_context ?bit_rate ?(frame_rate=25) id = create_context id false bit_rate (Some frame_rate)

  external decode : (input, video)context -> bytes -> int -> video frame array = "ocaml_avcodec_decode"
  external decode_data : (input, video)context -> data -> int -> video frame array = "ocaml_avcodec_decode_data"

  external encode : (output, video)context -> video frame -> bytes = "ocaml_avcodec_encode"
  external encode_to_data : (output, video)context -> video frame -> data = "ocaml_avcodec_encode_to_data"
end

(** Subtitle codecs. *)
module Subtitle = struct
  (** Subtitle codec ids *)
  type id = Codec_id.subtitle

  external get_name : id -> string = "ocaml_avcodec_get_subtitle_codec_name"

  external find_id : string -> id = "ocaml_avcodec_find_subtitle_codec_id"

  external get_id : subtitle t -> id = "ocaml_avcodec_parameters_get_subtitle_codec_id"
end
