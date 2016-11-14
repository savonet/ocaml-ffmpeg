#include <libswresample/swresample.h>

#include "avutil_stubs.h"

typedef enum _vector_kind {Str, P_Str, Fa, P_Fa, Ba, P_Ba, Frm} vector_kind;

typedef struct swr_t swr_t;

swr_t * swresample_create(vector_kind in_vector_kind, int64_t in_channel_layout, enum AVSampleFormat in_sample_fmt, int in_sample_rate,
			vector_kind out_vector_kind, int64_t out_channel_layout, enum AVSampleFormat out_sample_fmt, int out_sample_rate);

