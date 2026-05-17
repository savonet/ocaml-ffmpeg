module C = Configurator.V1

let () =
  C.main ~name:"ffmpeg-swscale-pkg-config" (fun c ->
      let default : C.Pkg_config.package_conf =
        { libs = ["-lswscale"]; cflags = [] }
      in
      let conf =
        match C.Pkg_config.get c with
          | None -> default
          | Some pc -> (
              match
                C.Pkg_config.query_expr_err pc ~package:"libswscale"
                  ~expr:"libswscale >= 4.8.100"
              with
                | Error msg -> failwith msg
                | Ok deps -> deps)
      in
      let system = Option.value ~default:"" (C.ocaml_config_var c "system") in
      let bsymbolic =
        if String.length system >= 5 && String.sub system 0 5 = "linux" then
          ["-Wl,-Bsymbolic"]
        else []
      in
      C.Flags.write_sexp "c_flags.sexp" conf.cflags;
      C.Flags.write_sexp "c_library_flags.sexp" (conf.libs @ bsymbolic))
