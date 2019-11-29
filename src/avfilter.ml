(** This module provides filters. *)

type graph_state = [
  | `Unconfigured
  | `Configured
]

type 'a t

(* Make sure graph is freed only after all filters are out of reach. *)

type filter_ctx

type filter_type = [
  | `Buffer
  | `Link
  | `Sink
]

type ('a, 'b, 'c, 'd) pad = {
  pad_name:       string;
  media_type:     'd;
  idx:            int;
  filter_ctx:     filter_ctx option;
  graph:          [`Unconfigured] t option
}

type ('a,'b, 'c) pads = {
  video: ('a, 'b, 'c, [`Video]) pad list;
  audio: ('a, 'b, 'c, [`Audio]) pad list
}

type ('a,'b) filter = {
  name:        string;
  description: string;
  inputs:  ('a, 'b, [`Input]) pads;
  outputs: ('a, 'b, [`Output]) pads;
}

type ('a, 'b, 'c, 'd) _filter = {
  _name:        string;
  _description: string;
  _inputs:  ('a, 'b, 'c, 'd) pad array;
  _outputs: ('a, 'b, 'c, 'd) pad array
}

external register_all : unit -> unit = "ocaml_avfilter_register_all"

let () = register_all ()

external finalize_graph : _ t -> unit = "ocaml_avfilter_finalize_filter_graph"

let () =
  Callback.register "ocaml_avfilter_finalize_filter_graph" finalize_graph

external get_all_filters : unit -> ([`Unattached], 'a, 'c, 'd) _filter array = "ocaml_avfilter_get_all_filters" 

let filters, buffers, sinks =
  let split_pads pads =
    let audio, video = Array.fold_left (fun (a,v) pad ->
      if pad.media_type = `Audio then
        let pad = {pad with media_type = `Audio} in
        pad::a, v
      else
        let pad = {pad with media_type = `Video} in
        a, pad::v) ([],[]) pads
    in

    let audio =
      List.sort (fun pad1 pad2 -> compare pad1.idx pad2.idx) audio
    in
    let video =
      List.sort (fun pad1 pad2 -> compare pad1.idx pad2.idx) video
    in

    {audio; video}
  in

  let _buffers =
    [("buffer", `Video); ("abuffer", `Audio)]
  in
  let _sinks =
    [("buffersink", `Video) ; ("abuffersink", `Audio)]
  in

  let (filters, buffers, sinks) =
    Array.fold_left (fun (filters, buffers, sinks) {_name;_description;_inputs;_outputs} ->
     let filter = {name=_name;description=_description;
        inputs=split_pads _inputs;
        outputs=split_pads _outputs
      } in
      if List.mem_assoc _name _buffers then
        (filters, filter::buffers, sinks)
      else if List.mem_assoc _name _sinks then
        (filters, buffers, filter::sinks)
      else
        (filter::filters, buffers, sinks)) ([],[],[]) (get_all_filters ())
  in

  let sort = List.sort (fun f1 f2 -> compare f1.name f2.name) in

  (** Buffer and sinks do not have inputs (resp. outputs) in the FFMPEG API,
    * adding some here for our version of it. *)
  let buffers = List.map (fun filter ->
    let inputs =
      match List.assoc filter.name _buffers with
        | `Audio -> {
             video = [];
             audio = [{
               pad_name = "default";
               media_type = `Audio;
               idx = 0;
               filter_ctx = None;
               graph = None
            }]
          }
        | `Video -> {
             audio = [];
             video = [{
               pad_name = "default";
               media_type = `Video;
               idx = 0;
               filter_ctx = None;
               graph = None
            }]
          }
    in
    {filter with inputs}) buffers
  in

  let sinks= List.map (fun filter ->
    let outputs =
      match List.assoc filter.name _sinks with
        | `Audio -> {
             video = [];
             audio = [{
               pad_name = "default";
               media_type = `Audio;
               idx = 0;
               filter_ctx = None;
               graph = None
            }]
          }
        | `Video -> {
             audio = [];
             video = [{
               pad_name = "default";
               media_type = `Video;
               idx = 0;
               filter_ctx = None;
               graph = None
            }]
          }
    in
    {filter with outputs}) sinks
  in

  sort filters, sort buffers, sort sinks

let pad_name {pad_name} = pad_name

external init : unit -> [`Unconfigured] t = "ocaml_avfilter_init"

external create_filter : ?name:string -> ?args:string -> string -> [`Unconfigured] t -> filter_ctx = "ocaml_avfilter_create_filter"

let attach ?name ?args filter graph =
  let filter_ctx =
    create_filter ?name ?args filter.name graph
  in
  let f () =
    List.map (fun pad -> {
      pad with
        filter_ctx = Some filter_ctx;
        graph = Some graph
    })
  in
  let inputs = {
    audio = (f ()) filter.inputs.audio;
    video = (f ()) filter.inputs.video
  } in
  let outputs = {
    audio = (f ()) filter.inputs.audio;
    video = (f ()) filter.inputs.video
  } in
  {filter with inputs; outputs}

external link : filter_ctx -> int -> filter_ctx -> int -> unit = "ocaml_avfilter_link" 

let get_some = function
  | Some x -> x
  | None -> assert false

let link src dst =
  link (get_some src.filter_ctx) src.idx (get_some dst.filter_ctx) dst.idx

external config : [`Unconfigured] t -> unit = "ocaml_avfilter_config"

let config graph =
  config graph;
  Obj.magic graph

external write_frame : filter_ctx -> 'a Avutil.frame -> unit = "ocaml_avfilter_write_frame"

let write_frame _ {filter_ctx} frame =
  write_frame (get_some filter_ctx) frame

external get_frame : filter_ctx -> 'b Avutil.frame = "ocaml_avfilter_get_frame"

let get_frame _ {filter_ctx} =
  get_frame (get_some filter_ctx)
