SOURCES = transcoding.ml
RESULT = transcoding
LIBS = bigarray ffmpeg

INCDIRS = ../../src
THREADS = yes

all: dnc

test: all
	./transcoding ../encode_audio/A4.ogg A4.mp4

include OCamlMakefile
