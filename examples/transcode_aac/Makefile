SOURCES = transcode_aac.ml
RESULT = transcode_aac
LIBS = bigarray ffmpeg

INCDIRS = ../../src
THREADS = yes

all: nc

test: all
	./transcode_aac ../encode_audio/A4.ogg A4.mp4

include OCamlMakefile
