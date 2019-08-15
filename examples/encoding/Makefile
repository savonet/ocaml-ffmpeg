SOURCES = encoding.ml
RESULT = encoding
LIBS = bigarray ffmpeg

INCDIRS = ../../src
THREADS = yes
CFLAGS = -O3
OCAMLFLAGS = -unsafe -inline 10 -noassert
TRASH = *.raw *.flac *.ogg *.mp4 *.mkv

all: nc

test: all
	./encoding out.mkv aac mpeg4 ass

include OCamlMakefile
