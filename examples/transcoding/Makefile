SOURCES = transcoding.ml
RESULT = transcoding
LIBS = bigarray ffmpeg

INCDIRS = ../../src
THREADS = yes
TRASH = *.raw *.flac *.ogg *.mp4 *.mkv

all: dnc

test: all
	./transcoding ../encoding/out.mkv out.mp4

include OCamlMakefile
