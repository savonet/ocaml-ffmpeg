SOURCES = audio_device.ml
RESULT = audio_device
LIBS = bigarray ffmpeg

INCDIRS = ../../src
THREADS = yes
TRASH = *.raw *.flac *.ogg *.mp4 *.mkv

all: dc

test: all
	./audio_device
	./audio_device http://37.187.70.49:80/canalb
	./audio_device http://www.jetfm.asso.fr/live jetfm.flac

include OCamlMakefile
