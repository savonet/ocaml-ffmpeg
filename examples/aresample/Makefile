SOURCES = aresample.ml
RESULT = aresample
LIBS = bigarray ffmpeg

INCDIRS = ../../src
THREADS = yes
TRASH = *.raw *.flac *.ogg *.mp4 *.mkv

all: nc

test: all
	./aresample ../encoding/out.mkv out.mp4

include OCamlMakefile
