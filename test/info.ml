open FFmpeg
open Avutil
open Printf

let test() =
  Sys.argv |> Array.to_list |> List.tl |> List.iter(fun url ->
      printf"%s :\n" url;
      let input = Av.Input.open_url url in

      Av.(get_streams_codec_parameters@@of_input input) |> Array.iter(function
          | Avcodec.Parameters.Audio cp -> Avcodec.Audio.(
              printf"\tAudio stream : %s %s, %s %s, %s %d, %s %d, %s %d\n"
                "codec" (Parameters.get_codec_id cp |> get_name)
                "sample format" (Parameters.get_sample_format cp |> Sample_format.get_name)
                "channels" (Parameters.get_nb_channels cp)
                "bit rate" (Parameters.get_bit_rate cp)
                "sample rate" (Parameters.get_sample_rate cp)
            )
          | Avcodec.Parameters.Video cp -> Avcodec.Video.(
              let sar_num, sar_den = Parameters.get_sample_aspect_ratio cp in
              printf"\tVideo stream : %s %s, %s %d, %s %d, %s %d / %d, %s %d\n"
                "codec" (Parameters.get_codec_id cp |> get_name)
                "width" (Parameters.get_width cp)
                "height" (Parameters.get_height cp)
                "sample aspect ratio" sar_num sar_den
                "bit rate" (Parameters.get_bit_rate cp)
            )
          | Avcodec.Parameters.Subtitle cp -> Avcodec.Subtitle.(
              printf"\tSubtitle stream : %s %s\n"
                "codec" (Parameters.get_codec_id cp |> get_name)
            )
          | Avcodec.Parameters.Unknown -> printf"\tUnknown stream\n"
        );
      printf"\n";
      Av.Input.get_metadata input |> List.iter(fun(k, v) -> printf"\t%s : %s\n" k v);
    );




