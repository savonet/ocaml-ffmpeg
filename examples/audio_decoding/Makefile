SOURCES = audio_decoding.ml
RESULT = audio_decoding
LIBS = bigarray ffmpeg

INCDIRS = ../../src
THREADS = yes
TRASH = *.raw *.flac *.ogg *.mp4 *.mkv

all: nc

test: all
	./audio_decoding ../decode_audio/A4.ogg A4
	./audio_decoding ../encoding/out.mkv out

include OCamlMakefile
