
external get_input_audio_devices : unit -> Av.Input.Format.t array = "ocaml_avdevice_get_input_audio_devices"
let get_input_audio_devices() = Array.to_list ( get_input_audio_devices() )

external get_input_video_devices : unit -> Av.Input.Format.t array = "ocaml_avdevice_get_input_video_devices"
let get_input_video_devices() = Array.to_list ( get_input_video_devices() )

external get_output_audio_devices : unit -> Av.Output.Format.t array = "ocaml_avdevice_get_output_audio_devices"
let get_output_audio_devices() = Array.to_list ( get_output_audio_devices() )

external get_output_video_devices : unit -> Av.Output.Format.t array = "ocaml_avdevice_get_output_video_devices"
let get_output_video_devices() = Array.to_list ( get_output_video_devices() )

module App_to_dev = struct
  type message_t =
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

  external control_message : message_t -> Av.t -> unit = "ocaml_avdevice_app_to_dev_control_message"
  let control_messages messages av = List.iter(fun msg -> control_message msg av) messages
end

module Dev_to_app = struct
  type message_t =
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

  external set_control_message_callback : (message_t -> unit) -> Av.t -> unit = "ocaml_avdevice_set_control_message_callback"
end
