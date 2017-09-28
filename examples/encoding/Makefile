SOURCES = encoding.ml
RESULT = encoding
LIBS = bigarray ffmpeg

INCDIRS = ../../src
THREADS = yes

all: dc

test: all
	./encoding out.mkv aac mpeg4 ass

include OCamlMakefile
