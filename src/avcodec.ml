open Avutil
module Ba = Bigarray.Array1

type 'a params
type 'media decoder
type 'media encoder

external init : unit -> unit = "ocaml_avcodec_init" [@@noalloc]

let () = init ()

external finalize_codec_parameters : 'a params -> unit = "ocaml_avcodec_finalize_codec_parameters"

let () =
  Callback.register "ocaml_avcodec_finalize_codec_parameters" finalize_codec_parameters


external get_input_buffer_padding_size : unit -> int = "ocaml_avcodec_get_input_buffer_padding_size"

let input_buffer_padding_size = get_input_buffer_padding_size()
let empty_data = create_data 0

(** Packet. *)
module Packet = struct
  (** Packet type *)
  type 'a t

  external finalize : 'a t -> unit = "ocaml_avcodec_finalize_packet"

  let () =
    Callback.register "ocaml_avcodec_finalize_packet" finalize

  external get_size : 'a t -> int = "ocaml_avcodec_get_packet_size"

  external get_stream_index : 'a t -> int = "ocaml_avcodec_get_packet_stream_index"
  external set_stream_index : 'a t -> int -> unit = "ocaml_avcodec_set_packet_stream_index"

  external to_bytes : 'a t -> bytes = "ocaml_avcodec_packet_to_bytes"


  type parser_t
  type 'a parser = {mutable buf:data; mutable remainder:data; parser:parser_t}

  external finalize_parser : parser_t -> unit = "ocaml_avcodec_finalize_parser"

  let () =
    Callback.register "ocaml_avcodec_finalize_parser" finalize_parser

  (* This is an internal function, which receives any type of AVCodec in the C code. *)
  external create_parser : 'a -> parser_t = "ocaml_avcodec_create_parser"

  let create_parser codec = {buf = empty_data; remainder = empty_data;
                          parser = create_parser codec}

  external parse_packet : parser_t -> data -> int -> int -> ('m t * int)option = "ocaml_avcodec_parse_packet"

  let rec buf_loop ctx f buf ofs len =
      match parse_packet ctx.parser buf ofs len with
      | Some(pkt, l) -> f pkt;
        buf_loop ctx f buf (ofs + l) (len - l)
      | None -> ofs

  let parse_data ctx f data =

    let remainder_len = Ba.dim ctx.remainder in
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

    let parsed_len = buf_loop ctx f buf 0 actual_len in

    ctx.buf <- buf;
    ctx.remainder <- Ba.sub buf parsed_len (actual_len - parsed_len)


  let parse_bytes ctx f bytes len =
    let data = create_data len in

    for i = 0 to len - 1 do 
      data.{i} <- int_of_char(Bytes.get bytes i)
    done;
    parse_data ctx f data
end

(* These functions receive a AVCodec on the C side. *)
external create_decoder : 'a -> bool -> int option -> int option -> _ decoder = "ocaml_avcodec_create_context"
external create_encoder : 'a -> bool -> int option -> int option -> _ encoder = "ocaml_avcodec_create_context"

(* There isn't much choice here.. *)
external finalize_codec_context : 'a -> unit = "ocaml_avcodec_finalize_codec_context"
let () =
  Callback.register "ocaml_avcodec_finalize_codec_context" finalize_codec_context


(** Audio codecs. *)
module Audio = struct
  (** Audio codec ids *)
  type 'a t
  type id = Codec_id.audio

  external get_id : _ t -> id = "ocaml_avcodec_get_audio_codec_id"

  external string_of_id : id -> string = "ocaml_avcodec_get_audio_codec_id_name"

  external find_encoder : string -> [`Encoder] t = "ocaml_avcodec_find_audio_encoder"

  external find_decoder : string -> [`Decoder] t = "ocaml_avcodec_find_audio_decoder"

  
  external get_supported_channel_layouts : _ t -> Avutil.Channel_layout.t list = "ocaml_avcodec_get_supported_channel_layouts"
  let get_supported_channel_layouts codec = List.rev(get_supported_channel_layouts codec)

  
  let find_best_channel_layout codec default =
    match get_supported_channel_layouts codec with
      | h::_ -> h
      | [] -> default

  
  external get_supported_sample_formats : _ t -> Avutil.Sample_format.t list = "ocaml_avcodec_get_supported_sample_formats"
  let get_supported_sample_formats codec =
    List.rev (get_supported_sample_formats codec)

  
  let find_best_sample_format codec default =
    match get_supported_sample_formats codec with
      | h::_ -> h
      | [] -> default

  
  external get_supported_sample_rates : _ t -> int list = "ocaml_avcodec_get_supported_sample_rates"
  let get_supported_sample_rates codec = List.rev(get_supported_sample_rates codec)

  
  let find_best_sample_rate codec default =
    match get_supported_sample_rates codec with
      | h::_ -> h
      | [] -> default
      

  external get_params_id : audio params -> id = "ocaml_avcodec_parameters_get_audio_codec_id"
  external get_channel_layout : audio params -> Avutil.Channel_layout.t = "ocaml_avcodec_parameters_get_channel_layout"
  external get_nb_channels : audio params -> int = "ocaml_avcodec_parameters_get_nb_channels"
  external get_sample_format : audio params -> Avutil.Sample_format.t = "ocaml_avcodec_parameters_get_sample_format"
  external get_bit_rate : audio params -> int = "ocaml_avcodec_parameters_get_bit_rate"
  external get_sample_rate : audio params -> int = "ocaml_avcodec_parameters_get_sample_rate"


  let create_parser = Packet.create_parser

  let create_decoder codec = create_decoder codec true None None

  let create_encoder ?bit_rate codec = create_encoder codec false bit_rate None
end


(** Video codecs. *)
module Video = struct
  type 'a t
  type id = Codec_id.video

  external get_id : _ t -> id = "ocaml_avcodec_get_video_codec_id"

  external string_of_id : id -> string = "ocaml_avcodec_get_video_codec_id_name"

  external find_encoder : string -> [`Encoder] t = "ocaml_avcodec_find_video_encoder"

  external find_decoder : string -> [`Decoder] t = "ocaml_avcodec_find_video_decoder"

  external get_supported_frame_rates : _ t-> Avutil.rational list = "ocaml_avcodec_get_supported_frame_rates"
  let get_supported_frame_rates codec =
    List.rev  (get_supported_frame_rates codec)

  
  let find_best_frame_rate codec default =
    match get_supported_frame_rates codec with
      | h::_ -> h
      | [] -> default

  
  external get_supported_pixel_formats : _ t -> Avutil.Pixel_format.t list = "ocaml_avcodec_get_supported_pixel_formats"
  let get_supported_pixel_formats codec =
    List.rev (get_supported_pixel_formats codec)


  let find_best_pixel_format codec default =
    match get_supported_pixel_formats codec with
      | h::_ -> h
      | [] -> default

  
  external get_params_id : video params -> id = "ocaml_avcodec_parameters_get_video_codec_id"
  external get_width : video params -> int = "ocaml_avcodec_parameters_get_width"
  external get_height : video params -> int = "ocaml_avcodec_parameters_get_height"
  external get_sample_aspect_ratio : video params -> Avutil.rational = "ocaml_avcodec_parameters_get_sample_aspect_ratio"
  external get_pixel_format : video params -> Avutil.Pixel_format.t = "ocaml_avcodec_parameters_get_pixel_format"
  external get_bit_rate : video params -> int = "ocaml_avcodec_parameters_get_bit_rate"

  let create_parser = Packet.create_parser

  let create_decoder codec = create_decoder codec true None None

  let create_encoder ?bit_rate ?(frame_rate=25) codec =
    create_encoder codec false bit_rate (Some frame_rate)
end

(** Subtitle codecs. *)
module Subtitle = struct
  type 'a t
  type id = Codec_id.subtitle

  external get_id : _ t -> id = "ocaml_avcodec_get_subtitle_codec_id"

  external string_of_id : id -> string = "ocaml_avcodec_get_subtitle_codec_id_name"

  external find_encoder : string -> [`Encoder] t = "ocaml_avcodec_find_subtitle_encoder"

  external find_decoder : string -> [`Decoder] t = "ocaml_avcodec_find_subtitle_decoder"

  external get_params_id : subtitle params -> id = "ocaml_avcodec_parameters_get_subtitle_codec_id"
end



external _send_packet : 'media decoder -> 'media Packet.t -> unit = "ocaml_avcodec_send_packet"
external _receive_frame : 'media decoder -> 'media frame option = "ocaml_avcodec_receive_frame"
external _flush_decoder : 'media decoder -> unit = "ocaml_avcodec_flush_decoder"

let rec receive_frame decoder f =
  match _receive_frame decoder with
  | Some frame -> f frame; receive_frame decoder f
  | None -> ()


let decode decoder f packet =
  _send_packet decoder packet;
  receive_frame decoder f

let flush_decoder decoder f =
  try
    _flush_decoder decoder;
    receive_frame decoder f
  with Avutil.Error `Eof -> ()

external _send_frame : 'media encoder -> 'media frame -> unit = "ocaml_avcodec_send_frame"
external _receive_packet : 'media encoder -> 'media Packet.t option = "ocaml_avcodec_receive_packet"
external _flush_encoder : 'media encoder -> unit = "ocaml_avcodec_flush_encoder"

let rec receive_packet encoder f =
  match _receive_packet encoder with
  | Some packet -> f packet; receive_packet encoder f
  | None -> ()


let encode encoder f frame =
  _send_frame encoder frame;
  receive_packet encoder f

let flush_encoder encoder f =
  (* First flush remaining packets. *)
  receive_packet encoder f;
  _flush_encoder encoder;
  receive_packet encoder f
