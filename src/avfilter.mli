(** This module provides an API to AVfilter. *)

type config

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

(** (attached/unattached, pad_type, input/output, audio/video) pad *)
type ('a, 'b, 'c, 'd) pad

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

exception Exists

(** Filter list. *)
val filters : ([`Unattached], [`Link]) filter list

(** Buffers (input) list. *)
val buffers : ([`Unattached], [`Buffer]) filter list

(** Sink (output) list. *)
val sinks : ([`Unattached], [`Sink]) filter list

(** Pad name. *)
val pad_name : _ pad -> string

(** Initiate a filter graph configuration. *)
val init : unit -> config

(** Attach a filter to a filter graph configuration. Raises [Exists] if there is already a filter by that name in the graph. *)
val attach : ?args:args list -> name:string -> ([`Unattached], 'a) filter -> config-> ([`Attached], 'a) filter

(** Link two filter pads. *)
val link : ([`Attached], [<`Buffer|`Link], [`Output], 'a) pad -> ([`Attached], [<`Link|`Sink], [`Input], 'a) pad -> unit

(** Check validity and configure all the links and formats in the graph and return its outputs and outputs. *)
val launch : config -> t
