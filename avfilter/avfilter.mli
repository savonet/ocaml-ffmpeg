(** This module provides an API to AVfilter. *)

open Avutil

type config

type valued_arg =
  [ `String of string | `Int of int | `Float of float | `Rational of rational ]

type args = [ `Key of string | `Pair of string * valued_arg ]
type ('a, 'b) av = { audio : 'a; video : 'b }
type ('a, 'b) io = { inputs : 'a; outputs : 'b }

(** (attached/unattached, audio/video, input/output) pad *)
type ('a, 'b, 'c) pad

type ('a, 'b) pads =
  (('a, [ `Audio ], 'b) pad list, ('a, [ `Video ], 'b) pad list) av

type 'a filter = {
  name : string;
  description : string;
  io : (('a, [ `Input ]) pads, ('a, [ `Output ]) pads) io;
}

type 'a input = 'a frame -> unit
type 'a output = unit -> 'a frame
type 'a entries = (string * 'a) list
type inputs = ([ `Audio ] input entries, [ `Video ] input entries) av
type outputs = ([ `Audio ] output entries, [ `Video ] output entries) av
type t = (inputs, outputs) io

exception Exists

(** Filter list. *)
val filters : [ `Unattached ] filter list

(** Buffers (input) list. *)
val buffers : [ `Unattached ] filter list

(** Sink (output) list. *)
val sinks : [ `Unattached ] filter list

(** Pad name. *)
val pad_name : _ pad -> string

(** Initiate a filter graph configuration. *)
val init : unit -> config

(** Attach a filter to a filter graph configuration. Raises [Exists] if there is
    already a filter by that name in the graph. *)
val attach :
  ?args:args list ->
  name:string ->
  [ `Unattached ] filter ->
  config ->
  [ `Attached ] filter

(** Link two filter pads. *)
val link :
  ([ `Attached ], 'a, [ `Output ]) pad ->
  ([ `Attached ], 'a, [ `Input ]) pad ->
  unit

(** Parse a graph described by a string and attach outputs/inputs to it. *)
type ('a, 'b, 'c) parse_node = {
  node_name : string;
  node_args : args list option;
  node_pad : ('a, 'b, 'c) pad;
}

type ('a, 'b) parse_av =
  ( ('a, [ `Audio ], 'b) parse_node list,
    ('a, [ `Video ], 'b) parse_node list )
  av

type 'a parse_io = (('a, [ `Input ]) parse_av, ('a, [ `Output ]) parse_av) io

val parse :
  [ `Unattached ] parse_io -> string -> config -> [ `Attached ] parse_io

(** Check validity and configure all the links and formats in the graph and
    return its outputs and outputs. *)
val launch : config -> t
