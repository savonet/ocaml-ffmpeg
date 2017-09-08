SOURCES = swresample.ml info.ml test.ml
RESULT = test
LIBS = bigarray ffmpeg

INCDIRS = ../src
THREADS = yes

all: dc

test: all
	./test ../examples/encode_audio/A4.ogg ../examples/transcode_aac/A4.mp4

include OCamlMakefile
