(** This module provides an API to AVfilter. *)

type filter_type = [
  | `Buffer
  | `Link
  | `Sink
]

type valued_arg = [
  | `String of string
  | `Int of int
  | `Float of float
  | `Rational of Avutil.rational
]

type args = [
  | `Key of string
  | `Pair of (string * valued_arg)
]

type ('a, 'b) av = {
  audio: 'a;
  video: 'b
}

type ('a, 'b) io = {
  inputs: 'a;
  outputs: 'b
}

type _config
type filter_ctx

type ('a, 'b, 'c, 'd) pad = {
  pad_name:       string;
  media_type:     'd;
  idx:            int;
  filter_ctx:     filter_ctx option;
  _config:        _config option
}

type ('a,'b, 'c) pads =
  (('a, 'b, 'c, [`Audio]) pad list,
   ('a, 'b, 'c, [`Video]) pad list) av

type ('a,'b) filter = {
  name:        string;
  description: string;
  io:          (('a, 'b, [`Input]) pads,
                ('a, 'b, [`Output]) pads) io
}

type 'a input = 'a Avutil.frame -> unit
type 'a output = unit -> 'a Avutil.frame

type 'a entries = (string * 'a) list

type inputs =
  ([`Audio] input entries,
   [`Video] input entries) av

type outputs =
  ([`Audio] output entries,
   [`Video] output entries) av

type t = (inputs, outputs) io

type config = {
  c: _config;
  mutable names: string list;
  mutable video_inputs: filter_ctx entries;
  mutable audio_inputs: filter_ctx entries;
  mutable video_outputs: filter_ctx entries;
  mutable audio_outputs: filter_ctx entries
}

exception Exists

type ('a, 'b, 'c, 'd) _filter = {
  _name:        string;
  _description: string;
  _inputs:  ('a, 'b, 'c, 'd) pad array;
  _outputs: ('a, 'b, 'c, 'd) pad array
}

external register_all : unit -> unit = "ocaml_avfilter_register_all"

let () = register_all ()

external finalize_graph : _config -> unit = "ocaml_avfilter_finalize_filter_graph"

let () =
  Callback.register "ocaml_avfilter_finalize_filter_graph" finalize_graph

external get_all_filters : unit -> ([`Unattached], 'a, 'c, 'd) _filter array = "ocaml_avfilter_get_all_filters" 

let _buffers =
  [("buffer", `Video); ("abuffer", `Audio)]

let _sinks =
  [("buffersink", `Video) ; ("abuffersink", `Audio)]

let filters, buffers, sinks =
  let split_pads pads =
    let audio, video = Array.fold_left (fun (a,v) pad ->
      if pad.media_type = `Audio then
        let pad : (_, _, _, [`Audio]) pad = {pad with media_type = `Audio} in
        pad::a, v
      else
        let pad : (_, _, _, [`Video]) pad = {pad with media_type = `Video} in
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

  let (filters, buffers, sinks) =
    Array.fold_left (fun (filters, buffers, sinks) {_name;_description;_inputs;_outputs} ->
     let io = {
       inputs=split_pads _inputs;
       outputs=split_pads _outputs
     } in
     let filter = {
        name=_name;
        description=_description;
        io
      } in
      if List.mem_assoc _name _buffers then
        (filters, filter::buffers, sinks)
      else if List.mem_assoc _name _sinks then
        (filters, buffers, filter::sinks)
      else
        (filter::filters, buffers, sinks)) ([],[],[]) (get_all_filters ())
  in

  let sort = List.sort (fun f1 f2 -> compare f1.name f2.name) in

  sort filters, sort buffers, sort sinks

let pad_name {pad_name} = pad_name

external init : unit -> _config = "ocaml_avfilter_init"

let init () = {
  c = init();
  names = [];
  audio_inputs = [];
  video_inputs = [];
  audio_outputs = [];
  video_outputs = []
}

external create_filter : ?args:string -> name:string -> string -> _config -> filter_ctx = "ocaml_avfilter_create_filter"

let rec args_of_args cur = function
  | [] ->
       cur
  | (`Key s)::args ->
       args_of_args (s::cur) args
  | (`Pair (lbl, `String s))::args ->
       args_of_args
         ((Printf.sprintf "%s=%s" lbl s)::cur)
         args
  | (`Pair (lbl, `Int i))::args ->
       args_of_args
         ((Printf.sprintf "%s=%i" lbl i)::cur)
         args
  | (`Pair (lbl, `Float f))::args ->
       args_of_args
         ((Printf.sprintf "%s=%f" lbl f)::cur)
         args
  | (`Pair (lbl, `Rational {Avutil.num;den}))::args ->
       args_of_args
         ((Printf.sprintf "%s=%i/%i" lbl num den)::cur)
         args

let attach ?args ~name filter graph =
  if List.mem name graph.names then
    raise Exists;
  let args =
    match args with
      | Some args ->
          Some (String.concat ":" (args_of_args [] args))
      | None ->
          None
  in
  let filter_ctx =
    create_filter ?args ~name filter.name graph.c
  in
  let f () =
    List.map (fun pad -> {
      pad with
        filter_ctx = Some filter_ctx;
        _config = Some graph.c
    })
  in
  let inputs = {
    audio = (f ()) filter.io.inputs.audio;
    video = (f ()) filter.io.inputs.video
  } in
  let outputs = {
    audio = (f ()) filter.io.outputs.audio;
    video = (f ()) filter.io.outputs.video
  } in
  let io = {
    inputs;outputs
  } in
  let filter =
    {filter with io}
  in
  graph.names <- name::graph.names;
  begin
   match List.assoc_opt filter.name _buffers with
     | Some `Audio ->
           graph.audio_inputs <- (filter.name, filter_ctx)::graph.audio_inputs
     | Some `Video ->
           graph.video_inputs <- (filter.name, filter_ctx)::graph.video_inputs
     | None -> ()
  end;
  begin
   match List.assoc_opt filter.name _sinks with
     | Some `Audio ->
           graph.audio_outputs <- (filter.name, filter_ctx)::graph.audio_outputs
     | Some `Video ->
           graph.video_outputs <- (filter.name, filter_ctx)::graph.video_outputs
     | None -> ()
  end;
  filter

external link : filter_ctx -> int -> filter_ctx -> int -> unit = "ocaml_avfilter_link" 

let get_some = function
  | Some x -> x
  | None -> assert false

let link src dst =
  link (get_some src.filter_ctx) src.idx (get_some dst.filter_ctx) dst.idx

external config : _config -> unit = "ocaml_avfilter_config"

external write_frame : filter_ctx -> 'a Avutil.frame -> unit = "ocaml_avfilter_write_frame"

external get_frame : filter_ctx -> 'b Avutil.frame = "ocaml_avfilter_get_frame"

let launch graph =
  config graph.c;
  let audio = List.map (fun (name, filter_ctx) ->
    name, write_frame filter_ctx) graph.audio_inputs
  in
  let video = List.map (fun (name, filter_ctx) ->
    name, write_frame filter_ctx) graph.video_inputs
  in
  let inputs = {audio;video} in
  let audio = List.map (fun (name, filter_ctx) ->
    name, fun () -> get_frame filter_ctx) graph.audio_outputs
  in
  let video = List.map (fun (name, filter_ctx) ->
    name, fun () -> get_frame filter_ctx) graph.video_outputs
  in
  let outputs = {audio;video} in
  {outputs;inputs}
