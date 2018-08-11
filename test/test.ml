
let () =
  FFmpeg.Avutil.Log.set_level `Debug;
  FFmpeg.Avutil.Log.set_callback print_string;
  
  Resample.test();
  Info.test();
