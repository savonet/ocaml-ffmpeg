(lang dune 3.6)

(name ffmpeg)
(version 1.2.5)
(source (github savonet/ocaml-ffmpeg))
(license LGPL-2.1-only)
(authors "The Savonet Team <savonet-users@lists.sourceforge.net>")
(maintainers "Romain Beauxis <toots@rastageeks.org>")

(generate_opam_files true)
(use_standard_c_and_cxx_flags false)

(package
 (name ffmpeg-avutil)
 (synopsis "Bindings for the ffmpeg avutil libraries")
 (depends
  (conf-pkg-config :build)
  (conf-ffmpeg :build)
  (ocaml (>= 4.08.0))
  dune
  (dune-configurator :build)
  base-threads)
 (conflicts
  (ffmpeg (< 0.5.0)))
)

(package
 (name ffmpeg-avcodec)
 (synopsis "Bindings for the ffmpeg avcodec library")
 (depends
  (conf-pkg-config :build)
  (conf-ffmpeg :build)
  (ocaml (>= 4.08.0))
  dune
  (dune-configurator :build)
  (ffmpeg-avutil (= :version)))
 (conflicts
  (ffmpeg (< 0.5.0)))
)

(package
 (name ffmpeg-avfilter)
 (synopsis "Bindings for the ffmpeg avfilter library")
 (depends
  (conf-pkg-config :build)
  (conf-ffmpeg :build)
  (ocaml (>= 4.08.0))
  dune
  (dune-configurator :build)
  (ffmpeg-avutil (= :version)))
 (conflicts
  (ffmpeg (< 0.5.0)))
)

(package
 (name ffmpeg-swscale)
 (synopsis "Bindings for the ffmpeg swscale library")
 (depends
  (conf-pkg-config :build)
  (conf-ffmpeg :build)
  (ocaml (>= 4.08.0))
  dune
  (dune-configurator :build)
  (ffmpeg-avutil (= :version)))
 (conflicts
  (ffmpeg (< 0.5.0)))
)

(package
 (name ffmpeg-swresample)
 (synopsis "Bindings for the ffmpeg swresample library")
 (depends
  (conf-pkg-config :build)
  (conf-ffmpeg :build)
  (ocaml (>= 4.08.0))
  dune
  (dune-configurator :build)
  (ffmpeg-avutil (= :version))
  (ffmpeg-avcodec (= :version)))
 (conflicts
  (ffmpeg (< 0.5.0)))
)

(package
 (name ffmpeg-av)
 (synopsis "Bindings for the ffmpeg libraries -- top-level helpers")
 (depends
  (conf-pkg-config :build)
  (conf-ffmpeg :build)
  (ocaml (>= 4.08.0))
  dune
  (dune-configurator :build)
  (ffmpeg-avutil (= :version))
  (ffmpeg-avcodec (= :version)))
 (conflicts
  (ffmpeg (< 0.5.0)))
)

(package
 (name ffmpeg-avdevice)
 (synopsis "Bindings for the ffmpeg avdevice library")
 (depends
  (conf-pkg-config :build)
  (conf-ffmpeg :build)
  (ocaml (>= 4.08.0))
  dune
  (dune-configurator :build)
  (ffmpeg-av (= :version)))
 (conflicts
  (ffmpeg (< 0.5.0)))
)

(package
 (name ffmpeg)
 (synopsis "Bindings for the ffmpeg libraries")
 (depends
  (ocaml (>= 4.08.0))
  dune
  (ffmpeg-av (= :version))
  (ffmpeg-avutil (= :version))
  (ffmpeg-avcodec (= :version))
  (ffmpeg-avfilter (= :version))
  (ffmpeg-avdevice (= :version))
  (ffmpeg-swscale (= :version))
  (ffmpeg-swresample (= :version)))
)
