let () =
  Avutil.Log.set_level `Debug;
  Avutil.Log.set_callback print_string;

  Resample.test ();
  Info.test ()
