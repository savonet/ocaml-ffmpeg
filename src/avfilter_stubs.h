#include <caml/mlvalues.h>
#include <libavfilter/avfilter.h>

#define Filter_val(v) (*(struct AVFilter**)Data_custom_val(v))

#define InOut_val(v) (*(struct AVFilterInOut**)Data_custom_val(v))
