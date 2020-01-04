let () = Printexc.record_backtrace true

let () =
  assert (Array.length Sys.argv >= 2);

  Avutil.Log.set_level `Debug;
  Avutil.Log.set_callback print_string;

  let file = Sys.argv.(1) in
  let f = Av.open_input file in
  let md = Av.get_input_metadata f in
  Av.close f;
  List.iter (fun (l, v) -> Printf.printf "%s: %s\n" l v) md;

  Gc.full_major ()
