open Avutil

type filter

external get_by_name : string -> filter = "ocaml_avfilter_get_by_name"

type io

external create_io : unit -> io = "ocaml_avfilter_inout_alloc"
