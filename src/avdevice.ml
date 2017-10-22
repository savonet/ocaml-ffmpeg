open Avutil

external get_input_audio_devices : unit -> (input, audio)format array = "ocaml_avdevice_get_input_audio_devices"
let get_input_audio_devices() = Array.to_list ( get_input_audio_devices() )

external get_input_video_devices : unit -> (input, video)format array = "ocaml_avdevice_get_input_video_devices"
let get_input_video_devices() = Array.to_list ( get_input_video_devices() )

external get_output_audio_devices : unit -> (output, audio)format array = "ocaml_avdevice_get_output_audio_devices"
let get_output_audio_devices() = Array.to_list ( get_output_audio_devices() )

external get_output_video_devices : unit -> (output, video)format array = "ocaml_avdevice_get_output_video_devices"
let get_output_video_devices() = Array.to_list ( get_output_video_devices() )


let find_input name fmts =
  try List.find(fun d -> Av.Format.get_input_name d = name) fmts
  with Not_found -> raise(Failure("Input device not found : " ^ name))

let find_input_audio_device n = find_input n (get_input_audio_devices())

let find_input_video_device n = find_input n (get_input_video_devices())

let find_output name fmts =
  try List.find(fun d -> Av.Format.get_output_name d = name) fmts
  with Not_found -> raise(Failure("Output device not found : " ^ name))

let find_output_audio_device n = find_output n (get_output_audio_devices())

let find_output_video_device n = find_output n (get_output_video_devices())


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
