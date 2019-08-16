SOURCES = remuxing.ml
RESULT = remuxing
LIBS = bigarray ffmpeg

INCDIRS = ../../src
THREADS = yes
TRASH = *.raw *.flac *.ogg *.mp4 *.mkv

all: nc

test: all
	./remuxing ../encoding/out.mkv out.mp4


include OCamlMakefile
