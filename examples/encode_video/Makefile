SOURCES = encode_video.ml
RESULT = encode_video
LIBS = bigarray ffmpeg

INCDIRS = ../../src
THREADS = yes

all: dc

test: all
	./encode_video video.ogg libtheora

include OCamlMakefile
