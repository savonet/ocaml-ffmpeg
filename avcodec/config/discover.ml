module C = Configurator.V1

let has_bsf_h_code =
  Printf.sprintf {|
#include <libavcodec/bsf.h>

int main()
{
  return 0;
}
|}

let () =
  C.main ~name:"ffmpeg-avcodec-pkg-config" (fun c ->
      let default : C.Pkg_config.package_conf =
        { libs = ["-lavcodec"]; cflags = [] }
      in
      let conf =
        match C.Pkg_config.get c with
          | None -> default
          | Some pc -> (
              match
                C.Pkg_config.query_expr_err pc ~package:"libavcodec"
                  ~expr:"libavcodec >= 57.107.100"
              with
                | Error msg -> failwith msg
                | Ok deps -> deps)
      in
      C.Flags.write_sexp "c_flags.sexp" conf.cflags;
      C.Flags.write_lines "c_flags" conf.cflags;
      C.Flags.write_sexp "c_library_flags.sexp" conf.libs;

      let has_bsf_h =
        C.c_test ~c_flags:conf.cflags ~link_flags:conf.libs c has_bsf_h_code
      in

      C.C_define.gen_header_file c ~fname:"config.h"
        [("HAS_BSF_H", Switch has_bsf_h)])
