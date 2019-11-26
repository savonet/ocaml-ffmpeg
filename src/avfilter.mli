(** This module provides filters. *)

(** Type for a filter graph. *)
type 'a t

(** Type for a filter. *)
type 'a filter

(** Initiate a filter graph. *)
val init : unit -> 'a t

(** Create an attach a named filter to a filter graph. *)
val create_filter : ?instance_name:string -> ?args:string -> name:string -> 'a t -> 'a filter

(** Link two filters. *)
val link : 'a filter -> int -> 'a filter -> int -> unit

(** Check validity and configure all the links and formats in the graph. *)
val config : 'a t -> unit

(** Submit a frame to the filter graph. *)
val write_frame : 'a filter -> 'a Avutil.frame -> unit

(** Retrieve a frame from the filter graph. Raises [Avutil.Error `Eagain] if no frames are ready. *)
val get_frame : 'a filter -> 'a Avutil.frame

