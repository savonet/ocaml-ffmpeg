SOURCES = encode_audio.ml
RESULT = encode_audio
LIBS = bigarray ffmpeg

INCDIRS = ../../src
THREADS = yes

all: nc

test: all
	./encode_audio

include OCamlMakefile
