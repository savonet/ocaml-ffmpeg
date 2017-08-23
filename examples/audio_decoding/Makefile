SOURCES = audio_decoding.ml
RESULT = audio_decoding
LIBS = bigarray ffmpeg

INCDIRS = ../../src
THREADS = yes

all: dc

test: all
	./audio_decoding ../encode_audio/A4.ogg A4

include OCamlMakefile
