    
(** Return the audio input device list as input format. *)
val get_input_audio_devices : unit -> Av.Input.Format.t list
    
(** Return the video input device list as input format. *)
val get_input_video_devices : unit -> Av.Input.Format.t list
    
(** Return the audio output device list as output format. *)
val get_output_audio_devices : unit -> Av.Output.Format.t list
    
(** Return the video output device list as output format. *)
val get_output_video_devices : unit -> Av.Output.Format.t list

module App_to_dev : sig
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

  val control_messages : message_t list -> Av.t -> unit
end

module Dev_to_app : sig
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

  val set_control_message_callback : (message_t -> unit) -> Av.t -> unit
end
