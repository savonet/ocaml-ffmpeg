open Avutil

external get_audio_input_formats : unit -> (input, audio)format array = "ocaml_avdevice_get_audio_input_formats"
let get_audio_input_formats() = Array.to_list ( get_audio_input_formats() )
let get_default_audio_input_format() = List.hd(get_audio_input_formats())

external get_video_input_formats : unit -> (input, video)format array = "ocaml_avdevice_get_video_input_formats"
let get_video_input_formats() = Array.to_list ( get_video_input_formats() )
let get_default_video_input_format() = List.hd(get_video_input_formats())

external get_audio_output_formats : unit -> (output, audio)format array = "ocaml_avdevice_get_audio_output_formats"
let get_audio_output_formats() = Array.to_list ( get_audio_output_formats() )
let get_default_audio_output_format() = List.hd(get_audio_output_formats())

external get_video_output_formats : unit -> (output, video)format array = "ocaml_avdevice_get_video_output_formats"
let get_video_output_formats() = Array.to_list ( get_video_output_formats() )
let get_default_video_output_format() = List.hd(get_video_output_formats())


let find_input name fmts =
  try List.find(fun d -> Av.Format.get_input_name d = name) fmts
  with Not_found -> raise(Failure("Input device not found : " ^ name))

let find_audio_input name = find_input name (get_audio_input_formats())

let find_video_input name = find_input name (get_video_input_formats())

let find_output name fmts =
  try List.find(fun d -> Av.Format.get_output_name d = name) fmts
  with Not_found -> raise(Failure("Output device not found : " ^ name))

let find_audio_output name = find_output name (get_audio_output_formats())

let find_video_output name = find_output name (get_video_output_formats())


let open_audio_input name = Av.open_input_format(find_audio_input name)

let open_default_audio_input() =
  Av.open_input_format(get_default_audio_input_format())

let open_video_input name = Av.open_input_format(find_video_input name)

let open_default_video_input() =
  Av.open_input_format(get_default_video_input_format())

let open_audio_output name = Av.open_output_format(find_audio_output name)

let open_default_audio_output() =
  Av.open_output_format(get_default_audio_output_format())

let open_video_output name = Av.open_output_format(find_video_output name)

let open_default_video_output() =
  Av.open_output_format(get_default_video_output_format())



module App_to_dev = struct
  type message =
    | None
    | Window_size of int * int * int * int
    | Window_repaint of int * int * int * int
    | Pause
    | Play
    | Toggle_pause
    | Set_volume of float
    | Mute
    | Unmute
    | Toggle_mute
    | Get_volume
    | Get_mute

  external control_message : message -> _ container -> unit = "ocaml_avdevice_app_to_dev_control_message"
  let control_messages messages av = List.iter(fun msg -> control_message msg av) messages
end

module Dev_to_app = struct
  type message =
    | None
    | Create_window_buffer of (int * int * int * int) option
    | Prepare_window_buffer
    | Display_window_buffer
    | Destroy_window_buffer
    | Buffer_overflow
    | Buffer_underflow
    | Buffer_readable of Int64.t option
    | Buffer_writable of Int64.t option
    | Mute_state_changed of bool
    | Volume_level_changed of float

  external set_control_message_callback : (message -> unit) -> _ container -> unit = "ocaml_avdevice_set_control_message_callback"
end
