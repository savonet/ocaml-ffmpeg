SOURCES = audio_decoding.ml
RESULT = audio_decoding
LIBS = bigarray ffmpeg

INCDIRS = ../../src
THREADS = yes

all: dnc

test: all
	./audio_decoding ../../../ocaml-mm/examples/test.mp3

include OCamlMakefile
