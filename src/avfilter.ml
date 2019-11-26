(** This module provides filters. *)

type 'a t

type 'a _filter

(* Make sure graph is freed only after all filters are out of reach. *)
type 'a filter = {
  _filter: 'a _filter;
  graph: 'a t
}

external register_all : unit -> unit = "ocaml_avfilter_register_all"

let () = register_all ()

external finalize_graph : 'a t -> unit = "ocaml_avfilter_finalize_filter_graph"

let () =
  Callback.register "ocaml_avfilter_finalize_filter_graph" finalize_graph

external init : unit -> 'a t = "ocaml_avfilter_init"

external create_filter : ?instance_name:string -> ?args:string -> name:string -> 'a t -> 'a _filter = "ocaml_avfilter_create_filter"

let create_filter ?instance_name ?args ~name graph =
  let _filter = create_filter ?instance_name ?args ~name graph in
  {_filter;graph}

external link : 'a _filter -> int -> 'a _filter -> int -> unit = "ocaml_avfilter_link" 

let link src srcpad dst dstpad =
  link src._filter srcpad dst._filter dstpad

external config : 'a t -> unit = "ocaml_avfilter_config"

external write_frame : 'a _filter -> 'a Avutil.frame -> unit = "ocaml_avfilter_write_frame"

let write_frame {_filter} frame =
  write_frame _filter frame

external get_frame : 'a _filter -> 'a Avutil.frame = "ocaml_avfilter_get_frame"

let get_frame {_filter} = get_frame _filter
