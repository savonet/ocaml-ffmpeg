SOURCES = transcoding.ml
RESULT = transcoding
LIBS = bigarray ffmpeg

INCDIRS = ../../src
THREADS = yes

all: dnc

test: all
	./transcoding ../encoding/out.mkv out.mp4

include OCamlMakefile
