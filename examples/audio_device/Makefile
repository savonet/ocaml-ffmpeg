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
	./audio_device http://stream0.wfmu.org/freeform-128k wfmu.flac


include OCamlMakefile
