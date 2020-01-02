module C = Configurator.V1

let packages = ["avutil"; "avformat"; "swresample"; "swscale"]

let () =
  C.main ~name:"ffmpeg-av-pkg-config" (fun c ->
      let default : C.Pkg_config.package_conf =
        { libs = List.map (fun name -> "-l" ^ name) packages; cflags = [] }
      in
      let conf =
        match C.Pkg_config.get c with
          | None -> default
          | Some pc -> (
              match
                C.Pkg_config.query pc ~package:(String.concat " " packages)
              with
                | None -> default
                | Some deps -> deps )
      in
      C.Flags.write_sexp "c_flags.sexp" conf.cflags;
      C.Flags.write_sexp "c_library_flags.sexp" conf.libs)
