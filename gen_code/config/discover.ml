module C = Configurator.V1

let packages =
  [
    "libavutil";
    "libavformat";
    "libavfilter";
    "libavcodec";
    "libavdevice";
    "libswresample";
    "libswscale";
  ]

let default_path package =
  ["/usr/local/include" ^ package; "/usr/include" ^ package]

let trim s =
  match String.split_on_char '\n' (String.trim s) with s :: _ -> s | _ -> s

let split_flags flags =
  let l = String.split_on_char ' ' flags in
  List.fold_left
    (fun cur flag ->
      match String.split_on_char 'I' flag with
        | [_; inc] -> trim inc :: cur
        | _ -> cur)
    [] l

let add_path c pkg_config cur package =
  match C.Process.run c pkg_config ["--cflags-only-I"; package] with
    | { C.Process.exit_code; stdout; _ } when exit_code = 0 ->
        split_flags stdout @ cur
    | _ -> default_path package @ cur

let () =
  C.main ~name:"ffmpeg-gen_code-pkg-config" (fun c ->
      let paths =
        match C.which c "pkg-config" with
          | None ->
              List.fold_left
                (fun cur package -> default_path package @ cur)
                [] packages
          | Some pkg_config ->
              List.fold_left (add_path c pkg_config) [] packages
      in
      let paths =
        List.map
          (fun path -> String.trim (Printf.sprintf "%S" path))
          (List.sort_uniq compare paths)
      in
      Printf.printf "let paths = [%s]" (String.concat "; " paths))
