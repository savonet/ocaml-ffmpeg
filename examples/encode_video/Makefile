SOURCES = encode_video.ml
RESULT = encode_video
LIBS = bigarray ffmpeg

INCDIRS = ../../src
THREADS = yes
CFLAGS = -O3
OCAMLFLAGS = -O3 -unsafe -inline 100 -noassert
TRASH = *.raw *.flac *.ogg *.mp4 *.mkv

all: native-code

test: all
	./encode_video video.ogg libtheora

include OCamlMakefile
