open Av
    
(** Return the audio input device list as input format. *)
val get_input_audio_devices : unit -> Input.Format.t list
    
(** Return the video input device list as input format. *)
val get_input_video_devices : unit -> Input.Format.t list
    
(** Return the audio output device list as output format. *)
val get_output_audio_devices : unit -> Output.Format.t list
    
(** Return the video output device list as output format. *)
val get_output_video_devices : unit -> Output.Format.t list
