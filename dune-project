(lang dune 2.0)

(name ffmpeg)
(version 1.0.0)
(source (github savonet/ocaml-ffmpeg))
(license LGPL)
(authors "The Savonet Team <savonet-users@lists.sourceforge.net>")
(maintainers "Romain Beauxis <toots@rastageeks.org>")

(generate_opam_files true)

(package
 (name ffmpeg-avutil)
 (synopsis "Bindings for the ffmpeg avutil libraries")
 (depends
  (ocaml (>= 4.05.0))
  (dune (>= 2.0))
  dune-configurator
  base-threads)
 (conflicts
  (ffmpeg (< 0.5.0)))
)

(package
 (name ffmpeg-avcodec)
 (synopsis "Bindings for the ffmpeg avcodec library")
 (depends
  (ocaml (>= 4.05.0))
  (dune (>= 2.0))
  dune-configurator
  (ffmpeg-avutil (= :version)))
 (conflicts
  (ffmpeg (< 0.5.0)))
)

(package
 (name ffmpeg-avfilter)
 (synopsis "Bindings for the ffmpeg avfilter library")
 (depends
  (ocaml (>= 4.05.0))
  (dune (>= 2.0))
  dune-configurator
  (ffmpeg-avutil (= :version)))
 (conflicts
  (ffmpeg (< 0.5.0)))
)

(package
 (name ffmpeg-swscale)
 (synopsis "Bindings for the ffmpeg swscale library")
 (depends
  (ocaml (>= 4.05.0))
  (dune (>= 2.0))
  dune-configurator
  (ffmpeg-avutil (= :version)))
 (conflicts
  (ffmpeg (< 0.5.0)))
)

(package
 (name ffmpeg-swresample)
 (synopsis "Bindings for the ffmpeg swresample library")
 (depends
  (ocaml (>= 4.05.0))
  (dune (>= 2.0))
  dune-configurator
  (ffmpeg-avutil (= :version))
  (ffmpeg-avcodec (= :version)))
 (conflicts
  (ffmpeg (< 0.5.0)))
)

(package
 (name ffmpeg-av)
 (synopsis "Bindings for the ffmpeg libraries -- top-level helpers")
 (depends
  (ocaml (>= 4.05.0))
  (dune (>= 2.0))
  dune-configurator
  (ffmpeg-avutil (= :version))
  (ffmpeg-avcodec (= :version)))
 (conflicts
  (ffmpeg (< 0.5.0)))
)

(package
 (name ffmpeg-avdevice)
 (synopsis "Bindings for the ffmpeg avdevice library")
 (depends
  (ocaml (>= 4.05.0))
  (dune (>= 2.0))
  dune-configurator
  (ffmpeg-av (= :version)))
 (conflicts
  (ffmpeg (< 0.5.0)))
)

(package
 (name ffmpeg)
 (synopsis "Bindings for the ffmpeg libraries")
 (depends
  (ocaml (>= 4.05.0))
  (dune (>= 2.0))
  (ffmpeg-avutil (= :version))
  (ffmpeg-avcodec (= :version))
  (ffmpeg-avfilter (= :version))
  (ffmpeg-avdevice (= :version))
  (ffmpeg-swscale (= :version))
  (ffmpeg-swresample (= :version)))
 (conflicts
  (liquidsoap (< 1.5.0)))
)
