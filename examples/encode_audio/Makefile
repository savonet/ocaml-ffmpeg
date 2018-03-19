SOURCES = encode_audio.ml
RESULT = encode_audio
LIBS = bigarray ffmpeg

INCDIRS = ../../src
THREADS = yes
TRASH = *.raw *.flac *.ogg *.mp2 *.mp4 *.mkv

all: dc

test: all
	./encode_audio A4.flac flac
	./encode_audio A4.mp2 mp2

include OCamlMakefile
