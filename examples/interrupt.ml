let () = Printexc.record_backtrace true

let () =
  Avutil.Log.set_callback (fun _ -> ());

  let interrupt_m = Mutex.create () in
  let should_interrupt = ref false in

  ignore
    (Thread.create
       (fun () ->
         Unix.sleepf 0.1;
         Mutex.lock interrupt_m;
         should_interrupt := true;
         Mutex.unlock interrupt_m)
       ());

  let interrupt () =
    Mutex.lock interrupt_m;
    let b = !should_interrupt in
    Mutex.unlock interrupt_m;
    b
  in

  let opts = Hashtbl.create 1 in
  Hashtbl.replace opts "listen" (`Int 1);

  (try
     ignore (Av.open_input ~interrupt ~opts "rtmp://localhost/foo");
     assert false
   with Avutil.Error `Exit -> ());

  (try
     ignore (Av.open_output ~interrupt "http://localhost/foo.mp3");
     assert false
   with Avutil.Error `Exit -> ());

  Gc.full_major ();
  Gc.full_major ()
