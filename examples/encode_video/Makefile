SOURCES = encode_video.ml
RESULT = encode_video
LIBS = bigarray ffmpeg

INCDIRS = ../../src
THREADS = yes
CFLAGS = -O3
OCAMLFLAGS = -unsafe -inline 100 -noassert
TRASH = *.raw *.flac *.ogg *.mp4 *.mkv

all: nc

test: all
	./encode_video video.ogg libtheora

include OCamlMakefile
