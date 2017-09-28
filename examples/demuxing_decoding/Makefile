SOURCES = demuxing_decoding.ml
RESULT = demuxing_decoding
LIBS = bigarray ffmpeg

INCDIRS = ../../src
THREADS = yes

all: dnc

test: all
	./demuxing_decoding ../encoding/out.mkv vo.raw ao.raw

include OCamlMakefile
