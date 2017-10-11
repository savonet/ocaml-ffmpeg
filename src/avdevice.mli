(** Special devices access *)

open Avutil

(** Return the audio input device list as input format. *)
val get_input_audio_devices : unit -> (input, audio) format list
    
(** Return the video input device list as input format. *)
val get_input_video_devices : unit -> (input, video) format list
    
(** Return the audio output device list as output format. *)
val get_output_audio_devices : unit -> (output, audio) format list
    
(** Return the video output device list as output format. *)
val get_output_video_devices : unit -> (output, video) format list

(** Application to device communication *)
module App_to_dev : sig
(** Application to device control messages *)
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

  (** Send a list of control message to the device *)
  val control_messages : message list -> _ container -> unit
end

(** Device to application communication *)
module Dev_to_app : sig
(** Device to application control messages *)
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

  (** Set the callback for device message reception *)
  val set_control_message_callback : (message -> unit) -> _ container -> unit
end
