SOURCES = player.ml
RESULT = player
LIBS = bigarray ffmpeg

INCDIRS = ../../src
THREADS = yes

all: dc

test: all
	./player ../encoding/out.mkv
	./player ../transcoding/out.mp4


include OCamlMakefile
