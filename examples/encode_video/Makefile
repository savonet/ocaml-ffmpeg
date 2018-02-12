#SOURCES = encode_video.ml
SOURCES = set.ml planar_set.ml bigarray_copy.ml bytes_copy.ml bigarray_scale.ml bytes_scale.ml wrap.ml unsafe_wrap.ml test_bench.ml
RESULT = encode_video
LIBS = bigarray ffmpeg

INCDIRS = ../../src
THREADS = yes
CFLAGS = -O3
OCAMLFLAGS = -O3 -ffast-math -unsafe -inline 100 -noassert

all: native-code
#all: profiling-native-code

test: all
	./encode_video video.ogg libtheora

include OCamlMakefile
