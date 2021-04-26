#ifndef RISA_STD_H_GUARD
#define RISA_STD_H_GUARD

#include "../vm/vm.h"

void std_register_core(VM* vm);
void std_register_io(VM* vm);
void std_register_debug(VM* vm);

#endif
