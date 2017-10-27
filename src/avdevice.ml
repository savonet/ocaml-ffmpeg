open Avutil

external get_audio_inputs : unit -> (input, audio)format array = "ocaml_avdevice_get_audio_inputs"
let get_audio_inputs() = Array.to_list ( get_audio_inputs() )

external get_video_inputs : unit -> (input, video)format array = "ocaml_avdevice_get_video_inputs"
let get_video_inputs() = Array.to_list ( get_video_inputs() )

external get_audio_outputs : unit -> (output, audio)format array = "ocaml_avdevice_get_audio_outputs"
let get_audio_outputs() = Array.to_list ( get_audio_outputs() )

external get_video_outputs : unit -> (output, video)format array = "ocaml_avdevice_get_video_outputs"
let get_video_outputs() = Array.to_list ( get_video_outputs() )


let find_input name fmts =
  try List.find(fun d -> Av.Format.get_input_name d = name) fmts
  with Not_found -> raise(Failure("Input device not found : " ^ name))

let find_audio_input n = find_input n (get_audio_inputs())

let find_video_input n = find_input n (get_video_inputs())

let find_output name fmts =
  try List.find(fun d -> Av.Format.get_output_name d = name) fmts
  with Not_found -> raise(Failure("Output device not found : " ^ name))

let find_audio_output n = find_output n (get_audio_outputs())

let find_video_output n = find_output n (get_video_outputs())


let open_audio_input n = Av.open_input_format(find_audio_input n)

let open_default_audio_input() =
  Av.open_input_format(List.hd(get_audio_inputs()))

let open_video_input n = Av.open_input_format(find_video_input n)

let open_default_video_input() =
  Av.open_input_format(List.hd(get_video_inputs()))

let open_audio_output n = Av.open_output_format(find_audio_output n)

let open_default_audio_output() =
  Av.open_output_format(List.hd(get_audio_outputs()))

let open_video_output n = Av.open_output_format(find_video_output n)

let open_default_video_output() =
  Av.open_output_format(List.hd(get_video_outputs()))



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
