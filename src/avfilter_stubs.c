#include <caml/mlvalues.h>
#include <caml/memory.h>
#include <caml/alloc.h>
#include <caml/fail.h>
#include <caml/callback.h>
#include <caml/custom.h>
#include <caml/bigarray.h>
#include <caml/threads.h>

#include "avfilter_stubs.h"

static struct custom_operations filter_ops =
  {
   "ocaml_avfilter_filter",
   custom_finalize_default,
   custom_compare_default,
   custom_hash_default,
   custom_serialize_default,
   custom_deserialize_default
  };

CAMLprim value ocaml_avfilter_get_by_name(value name)
{
  CAMLparam1(name);
  CAMLlocal1(ans);

  const AVFilter* f = avfilter_get_by_name(String_val(name));
  if (f == NULL) caml_raise_not_found();
  ans = caml_alloc_custom(&filter_ops, sizeof(AVFilter*), 0, 1);

  CAMLreturn(ans);
}
