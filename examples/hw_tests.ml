open Avcodec

exception Skip

let () =
  let codec_name = Sys.argv.(1) in
  try
    let codec =
      try Video.find_encoder_by_name codec_name
      with _ ->
        Printf.printf "encoder %s not available!\n" codec_name;
        raise Skip
    in
    Printf.printf "hw_config for %s:\n%s\n" codec_name
      (String.concat ",\n  "
         (List.map
            (fun { pix_fmt; methods; device_type } ->
              Printf.sprintf
                "{\n  pix_fmt: %s,\n  methods: %s,\n  device_type: %s\n}"
                ( match Avutil.Pixel_format.to_string pix_fmt with
                  | None -> "none"
                  | Some f -> f )
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
                  | _ -> "not supported in this test!" ))
            (hw_configs codec)))
  with Skip -> ()
