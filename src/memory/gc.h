#ifndef RISA_MEM_GC_H_GUARD
#define RISA_MEM_GC_H_GUARD

#include "../vm/vm.h"

void gc_check(VM* vm);
void gc_run(VM* vm);

#endif
