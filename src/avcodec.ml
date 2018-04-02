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

  external parse_packet : parser_t -> data -> int -> int -> ('m t * int)option = "ocaml_avcodec_parse_packet"

  let rec buf_loop ctx f buf ofs len pkt_size =
    if len > pkt_size then (
      match parse_packet ctx.parser buf ofs len with
      | Some(pkt, l) -> f pkt;
        buf_loop ctx f buf (ofs + l) (len - l) (l + 16)
      | None -> ofs
    ) else ofs


  let parse_data ctx f data =

    let remainder_len = Ba.dim ctx.remainder in

    let data = if remainder_len = 0 then (
        let data_len = Ba.dim data in
        let ofs = buf_loop ctx f data 0 (data_len - input_buffer_padding_size) 0 in
        Ba.sub data ofs (data_len - ofs)
      )
      else data in

    let data_len = Ba.dim data in
    let actual_len = remainder_len + data_len in
    let needed_len = actual_len + input_buffer_padding_size in
    let buf_len = Ba.dim ctx.buf in

    let buf = if needed_len > buf_len then create_data needed_len
      else ctx.buf in

    if remainder_len > 0 then
      Ba.blit ctx.remainder (Ba.sub buf 0 remainder_len);

    Ba.blit data (Ba.sub buf remainder_len data_len);

    if needed_len <> buf_len then
      Ba.fill(Ba.sub buf actual_len input_buffer_padding_size) 0;

    let parsed_len = buf_loop ctx f buf 0 actual_len (-1) in

    ctx.buf <- buf;
    ctx.remainder <- Ba.sub buf parsed_len (actual_len - parsed_len)


  let parse_bytes ctx f bytes len =
    let data = create_data len in

    for i = 0 to len - 1 do 
      data.{i} <- int_of_char(Bytes.get bytes i)
    done;
    parse_data ctx f data

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



type send_result = [`Ok | `Again]

external _send_packet : 'media decoder -> 'media Packet.t -> send_result = "ocaml_avcodec_send_packet"
external _receive_frame : 'media decoder -> 'media frame option = "ocaml_avcodec_receive_frame"
external _flush_decoder : 'media decoder -> unit = "ocaml_avcodec_flush_decoder"

let rec receive_frame decoder f =
  match _receive_frame decoder with
  | Some frame -> f frame; receive_frame decoder f
  | None -> ()


let rec decode decoder f packet =
  match _send_packet decoder packet with
  | `Ok -> receive_frame decoder f
  | `Again -> receive_frame decoder f; decode decoder f packet


let flush_decoder decoder f =
  _flush_decoder decoder;
  receive_frame decoder f


external _send_frame : 'media encoder -> 'media frame -> send_result = "ocaml_avcodec_send_frame"
external _receive_packet : 'media encoder -> 'media Packet.t option = "ocaml_avcodec_receive_packet"
external _flush_encoder : 'media encoder -> unit = "ocaml_avcodec_flush_encoder"

let rec receive_packet encoder f =
  match _receive_packet encoder with
  | Some packet -> f packet; receive_packet encoder f
  | None -> ()


let rec encode encoder f frame =
  match _send_frame encoder frame with
  | `Ok -> receive_packet encoder f
  | `Again -> receive_packet encoder f; encode encoder f frame


let flush_encoder encoder f =
  _flush_encoder encoder;
  receive_packet encoder f
