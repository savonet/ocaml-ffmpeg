open Av

external get_input_audio_devices : unit -> Input_format.t array = "ocaml_avdevice_get_input_audio_devices"
let get_input_audio_devices() = Array.to_list ( get_input_audio_devices() )
    
external get_input_video_devices : unit -> Input_format.t array = "ocaml_avdevice_get_input_video_devices"
let get_input_video_devices() = Array.to_list ( get_input_video_devices() )
    
external get_output_audio_devices : unit -> Output_format.t array = "ocaml_avdevice_get_output_audio_devices"
let get_output_audio_devices() = Array.to_list ( get_output_audio_devices() )
    
external get_output_video_devices : unit -> Output_format.t array = "ocaml_avdevice_get_output_video_devices"
let get_output_video_devices() = Array.to_list ( get_output_video_devices() )

    


  
