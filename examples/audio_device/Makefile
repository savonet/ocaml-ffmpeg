SOURCES = audio_device.ml
RESULT = audio_device
LIBS = bigarray ffmpeg

INCDIRS = ../../src
THREADS = yes

all: dc

test: all
	./audio_device http://37.187.70.49:80/canalb

include OCamlMakefile
