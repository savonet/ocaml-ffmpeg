SOURCES = transcode_aac.ml
RESULT = transcode_aac
LIBS = bigarray ffmpeg

INCDIRS = ../../src
THREADS = yes
TRASH = *.raw *.flac *.ogg *.mp4 *.mkv

all: nc

test: all
	./transcode_aac ../decode_audio/A4.ogg A4.mp4

include OCamlMakefile
