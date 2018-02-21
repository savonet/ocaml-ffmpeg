SOURCES = encode_audio.ml
RESULT = encode_audio
LIBS = bigarray ffmpeg

INCDIRS = ../../src
THREADS = yes
TRASH = *.raw *.flac *.ogg *.mp4 *.mkv

all: nc

test: all
	./encode_audio A4.ogg libvorbis

include OCamlMakefile
