SOURCES = fps.ml
RESULT = fps
LIBS = bigarray ffmpeg

INCDIRS = ../../src
THREADS = yes
TRASH = *.raw *.flac *.ogg *.mp4 *.mkv

all: nc

test: all
	./fps ../encoding/out.mkv out.mp4

include OCamlMakefile
