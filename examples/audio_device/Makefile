SOURCES = audio_device.ml
RESULT = audio_device
LIBS = bigarray ffmpeg

INCDIRS = ../../src
THREADS = yes

all: dc

test: all
	./audio_device ../encode_audio/A4.ogg

include OCamlMakefile
