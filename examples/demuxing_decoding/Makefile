SOURCES = demuxing_decoding.ml
RESULT = demuxing_decoding
LIBS = bigarray ffmpeg

INCDIRS = ../../src
THREADS = yes

all: dnc

test: all
	./demuxing_decoding ../../../ocaml-mm/examples/test.mp3 vo.raw ao.raw

include OCamlMakefile
