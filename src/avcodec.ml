open Avutil
module Ba = Bigarray.Array1
              
type 'a t
type 'media decoder
type 'media encoder


external get_input_buffer_padding_size : unit -> int = "ocaml_avcodec_get_input_buffer_padding_size"
  
let input_buffer_padding_size = get_input_buffer_padding_size()
let empty_data = create_data 0

(** Packet. *)
module Packet = struct
  (** Packet type *)
  type 'a t

  external get_size : 'a t -> int = "ocaml_avcodec_get_packet_size"

  external get_stream_index : 'a t -> int = "ocaml_avcodec_get_packet_stream_index"
  external set_stream_index : 'a t -> int -> unit = "ocaml_avcodec_set_packet_stream_index"

  external to_bytes : 'a t -> bytes = "ocaml_avcodec_packet_to_bytes"

  
  type parser_t
  type 'a parser = {mutable buf:data; mutable remainder:data; parser:parser_t}

  external create_parser : int -> parser_t = "ocaml_avcodec_create_parser"

  let create_parser id = {buf = empty_data; remainder = empty_data;
                          parser = create_parser id}

  external parse_to_packets : parser_t -> data -> int -> 'm t array * int = "ocaml_avcodec_parse_to_packets"

  let parse_data ctx data len =
    let remaining_len = Ba.dim ctx.remainder in
    let actual_len = remaining_len + len in
    let needed_len = actual_len + input_buffer_padding_size in

    let buf = if needed_len > Ba.dim ctx.buf then create_data needed_len
      else ctx.buf in

    Ba.fill(Ba.sub buf actual_len input_buffer_padding_size) 0;

    if remaining_len > 0 then
      Ba.blit ctx.remainder (Ba.sub buf 0 remaining_len);

    Ba.blit(Ba.sub data 0 len)(Ba.sub buf remaining_len len);

    let packets, parsed_len = parse_to_packets ctx.parser buf actual_len in
    ctx.buf <- buf;
    ctx.remainder <- Ba.sub buf parsed_len (actual_len - parsed_len);
    packets

  
  let parse_bytes ctx bytes len =
    let data = create_data len in

    for i = 0 to len - 1 do 
      data.{i} <- int_of_char(Bytes.get bytes i)
    done;
    parse_data ctx data len
end

external create_decoder : int -> bool -> int option -> int option -> _ decoder = "ocaml_avcodec_create_context"
external create_encoder : int -> bool -> int option -> int option -> _ encoder = "ocaml_avcodec_create_context"


(** Audio codecs. *)
module Audio = struct
  (** Audio codec ids *)
  type id = Codec_id.audio

  external id_to_int : id -> int = "ocaml_avcodec_audio_codec_id_to_AVCodecID"

  external get_name : id -> string = "ocaml_avcodec_get_audio_codec_name"

  external find_id : string -> id = "ocaml_avcodec_find_audio_codec_id"

  external find_best_sample_format : id -> Avutil.Sample_format.t = "ocaml_avcodec_find_best_sample_format"

  external get_id : audio t -> id = "ocaml_avcodec_parameters_get_audio_codec_id"
  external get_channel_layout : audio t -> Avutil.Channel_layout.t = "ocaml_avcodec_parameters_get_channel_layout"
  external get_nb_channels : audio t -> int = "ocaml_avcodec_parameters_get_nb_channels"
  external get_sample_format : audio t -> Avutil.Sample_format.t = "ocaml_avcodec_parameters_get_sample_format"
  external get_bit_rate : audio t -> int = "ocaml_avcodec_parameters_get_bit_rate"
  external get_sample_rate : audio t -> int = "ocaml_avcodec_parameters_get_sample_rate"


  let create_parser id = Packet.create_parser(id_to_int id)

  let create_decoder id = create_decoder (id_to_int id) true None None

  let create_encoder ?bit_rate id = create_encoder (id_to_int id) false bit_rate None
end

  
(** Video codecs. *)
module Video = struct
  (** Video codec ids *)
  type id = Codec_id.video

  external id_to_int : id -> int = "ocaml_avcodec_video_codec_id_to_AVCodecID"

  external get_name : id -> string = "ocaml_avcodec_get_video_codec_name"

  external find_id : string -> id = "ocaml_avcodec_find_video_codec_id"

  external find_best_pixel_format : id -> Avutil.Pixel_format.t = "ocaml_avcodec_find_best_pixel_format"

  external get_id : video t -> id = "ocaml_avcodec_parameters_get_video_codec_id"
  external get_width : video t -> int = "ocaml_avcodec_parameters_get_width"
  external get_height : video t -> int = "ocaml_avcodec_parameters_get_height"
  external get_sample_aspect_ratio : video t -> int * int = "ocaml_avcodec_parameters_get_sample_aspect_ratio"
  external get_pixel_format : video t -> Avutil.Pixel_format.t = "ocaml_avcodec_parameters_get_pixel_format"
  external get_bit_rate : video t -> int = "ocaml_avcodec_parameters_get_bit_rate"

  let create_parser id = Packet.create_parser(id_to_int id)

  let create_decoder id = create_decoder (id_to_int id) true None None

  let create_encoder ?bit_rate ?(frame_rate=25) id = create_encoder (id_to_int id) false bit_rate (Some frame_rate)
end

(** Subtitle codecs. *)
module Subtitle = struct
  (** Subtitle codec ids *)
  type id = Codec_id.subtitle

  (* external id_to_int : id -> int = "ocaml_avcodec_subtitle_codec_id_to_AVCodecID" *)

  external get_name : id -> string = "ocaml_avcodec_get_subtitle_codec_name"

  external find_id : string -> id = "ocaml_avcodec_find_subtitle_codec_id"

  external get_id : subtitle t -> id = "ocaml_avcodec_parameters_get_subtitle_codec_id"
end

external decode : 'media decoder -> 'media Packet.t -> 'media frame array = "ocaml_avcodec_decode"
external flush_decoder : 'media decoder -> 'media frame array = "ocaml_avcodec_flush_decoder"

external encode : 'media encoder -> 'media frame -> 'media Packet.t array = "ocaml_avcodec_encode"
external flush_encoder : 'media encoder -> 'media Packet.t array = "ocaml_avcodec_flush_encoder"
