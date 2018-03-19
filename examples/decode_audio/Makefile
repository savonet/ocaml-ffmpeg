SOURCES = decode_audio.ml
RESULT = decode_audio
LIBS = bigarray ffmpeg

INCDIRS = ../../src
THREADS = yes
TRASH = *.raw *.flac *.ogg *.mp3 *.mp4 *.mkv

all: nc

test: all
	./decode_audio ../encode_audio/A4.mp2 mp2 A4.ogg libvorbis
	./decode_audio ../encode_audio/A4.flac mp2 A4.mp3 libmp3lame


include OCamlMakefile
