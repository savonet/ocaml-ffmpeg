(** This module provides filters. *)

type graph_state = [
  | `Unconfigured
  | `Configured
]

type 'a t

type filter_type = [
  | `Buffer
  | `Link
  | `Sink
]

(** (attached/unattached, pad_type, input/output, audio/video) pad *)
type ('a, 'b, 'c, 'd) pad

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

(** Filter list. *)
val filters : ([`Unattached], [`Link]) filter list

(** Buffers (input) list. *)
val buffers : ([`Unattached], [`Buffer]) filter list

(** Sink (output) list. *)
val sinks : ([`Unattached], [`Sink]) filter list

(** Pad name. *)
val pad_name : _ pad -> string

(** Initiate a filter graph. *)
val init : unit -> [`Unconfigured] t

(** Attach a filter to a filter graph. *)
val attach : ?name:string -> ?args:string -> ([`Unattached], 'a) filter -> [`Unconfigured] t -> ([`Attached], 'a) filter

(** Link two filter pads. *)
val link : ([`Attached], [<`Buffer|`Link], [`Output], 'a) pad -> ([`Attached], [<`Link|`Sink], [`Input], 'a) pad -> unit

(** Check validity and configure all the links and formats in the graph. *)
val config : [`Unconfigured] t -> [`Configured] t

(** Submit a frame to the filter graph. *)
val write_frame : [`Configured] t -> ([`Attached], [`Buffer], [`Input], 'a) pad -> 'a Avutil.frame -> unit

(** Retrieve a frame from the filter graph. Raises [Avutil.Error `Eagain] if no frames are ready. *)
val get_frame : [`Configured] t -> ([`Attached], [`Sink], [`Output], 'a) pad -> 'a Avutil.frame

