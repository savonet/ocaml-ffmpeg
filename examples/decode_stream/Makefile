SOURCES = decode_stream.ml
RESULT = decode_stream
LIBS = bigarray ffmpeg
OCAMLFLAGS = -g

INCDIRS = ../../src
THREADS = yes
TRASH = *.raw *.flac *.ogg *.mp3 *.mp4 *.mkv

all: nc

test: all
	./decode_stream ../encode_audio/A4.flac flac A4.mp3 libmp3lame
	./decode_stream ../encode_audio/A4.mp2 mp2 A4.ogg libvorbis


include OCamlMakefile
