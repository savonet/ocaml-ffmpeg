#include <caml/mlvalues.h>
#include <libavfilter/avfilter.h>

#define Filter_val(v) (*(struct AVFilter**)Data_custom_val(v))

#define InOut_val_ptr(v) ((struct AVFilterInOut**)Data_custom_val(v))

#define InOut_val(v) (*InOut_val_ptr(v))

#define Context_val_ptr(v) ((struct AVFilterContext **)Data_custom_val(v))

#define Context_val(v) (*Context_val_ptr(v))
