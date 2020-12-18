open Avcodec

exception Skip

let () =
  let codec = "libopencore_amrwb" in
  try
    let codec =
      try Video.find_decoder_by_name codec
      with _ ->
        Printf.printf "codec %s not available!\n" codec;
        raise Skip
    in
    Printf.printf "hw_config for h264_videotoolbox:\n  %s\n"
      (String.concat ",\n  "
         (List.map
            (fun { pix_fmt; methods; device_type } ->
              Printf.sprintf "pix_fmt: %s, methods: %s, device_type: %s"
                (Avutil.Pixel_format.to_string pix_fmt)
                (String.concat ", "
                   (List.map
                      (function
                        | `Hw_device_ctx -> "hw_device_ctx"
                        | `Hw_frames_ctx -> "hw_frame_ctx"
                        | `Internal -> "internal"
                        | `Ad_hoc -> "ad-hoc")
                      methods))
                ( match device_type with
                  | `None -> "none"
                  | `Vdpau -> "vdpau"
                  | `Cuda -> "cuda"
                  | `Vaapi -> "vaapi"
                  | `Dxva2 -> "dxva2"
                  | `Qsv -> "qsv"
                  | `Videotoolbox -> "videotoolbox"
                  | `D3d11va -> "d3d11va"
                  | `Drm -> "drm"
                  | `Opencl -> "opencl"
                  | `Mediacodec -> "mediacodec"
                  | `Vulkan -> "vulkan" ))
            (hw_configs codec)))
  with Skip -> ()
