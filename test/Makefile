SOURCES = resample.ml info.ml test.ml
RESULT = test
LIBS = bigarray ffmpeg

INCDIRS = ../src
THREADS = yes
TRASH = *.raw *.flac *.ogg *.mp4 *.mkv

all: dc

test: all
	./test ../examples/decode_audio/A4.ogg ../examples/transcode_aac/A4.mp4 ../examples/encoding/out.mkv

include OCamlMakefile
