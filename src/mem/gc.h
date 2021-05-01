#ifndef RISA_MEM_GC_H_GUARD
#define RISA_MEM_GC_H_GUARD

#include "../vm/vm.h"

void risa_gc_check (RisaVM* vm);
void risa_gc_run   (RisaVM* vm);

#endif
