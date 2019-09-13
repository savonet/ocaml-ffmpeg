SOURCES = read_metadata.ml
RESULT = read_metadata
LIBS = bigarray ffmpeg

INCDIRS = ../../src
THREADS = yes
TRASH = *.raw *.flac *.ogg *.mp3 *.mp4 *.mkv

all: nc

test: all
	./read_metadata ../encode_audio/A4.flac flac A4.mp3 libmp3lame
	./read_metadata ../encode_audio/A4.mp2 mp2 A4.ogg libvorbis


include OCamlMakefile
