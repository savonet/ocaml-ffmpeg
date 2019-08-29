SOURCES = encode_stream.ml
RESULT = encode_stream
LIBS = bigarray ffmpeg

INCDIRS = ../../src
THREADS = yes
TRASH = *.raw *.flac *.ogg *.mp2 *.mp4 *.mkv

all: nc

test: all
	./encode_stream A4.flac flac
	./encode_stream A4.mp2 mp2

include OCamlMakefile
