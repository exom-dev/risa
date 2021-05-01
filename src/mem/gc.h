#ifndef RISA_MEM_GC_H_GUARD
#define RISA_MEM_GC_H_GUARD

#include "../api.h"
#include "../vm/vm.h"

RISA_API void risa_gc_check (RisaVM* vm);
RISA_API void risa_gc_run   (RisaVM* vm);

#endif
