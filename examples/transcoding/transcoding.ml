open FFmpeg

let () =
  if Array.length Sys.argv < 3 then (
    Printf.printf"usage: %s input_file output_file\n" Sys.argv.(0);
    exit 1);

  let src = Av.Input.open_url Sys.argv.(1) in
  let dst = Av.Output.open_file Sys.argv.(2) in

  let streams = Av.(src >- get_streams_codec_parameters) in
  let mapping = Array.make (Array.length streams) 0 in

  streams |> Array.iteri Avcodec.(fun i cp -> match cp with
      | Parameters.Audio cp -> mapping.(i) <- Av.Output.new_audio_stream
            ~codec_id:Audio.AC_AAC ~codec_parameters:cp dst
      | Parameters.Video cp -> mapping.(i) <- Av.Output.new_video_stream
              ~codec_id:Video.VC_H264 ~codec_parameters:cp dst
      | Parameters.Subtitle cp ->
          mapping.(i) <- Av.Output.new_subtitle_stream ~codec_parameters:cp dst
      | _ -> ());

  src |> Av.Input.iter
    ~audio:(fun ii frm -> Av.Output.write_audio_frame dst mapping.(ii) frm)
    ~video:(fun ii frm -> Av.Output.write_video_frame dst mapping.(ii) frm)
    ~subtitle:(fun ii frm -> Av.Output.write_subtitle_frame dst mapping.(ii) frm);

  Av.Input.close src;
  Av.Output.close dst;

  Gc.full_major ()
