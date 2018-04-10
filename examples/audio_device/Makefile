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
	./audio_device http://80.82.229.202/jetfm.mp3 jetfm.flac

include OCamlMakefile
