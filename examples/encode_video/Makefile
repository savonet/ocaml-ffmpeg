SOURCES = encode_video.ml
RESULT = encode_video
LIBS = bigarray ffmpeg

INCDIRS = ../../src
THREADS = yes

all: nc

test: all
	./encode_video video.ogg libtheora

include OCamlMakefile
