#ifndef RISA_STD_H_GUARD
#define RISA_STD_H_GUARD

#include "../vm/vm.h"

void risa_std_register_core  (RisaVM* vm);
void risa_std_register_io    (RisaVM* vm);
void risa_std_register_debug (RisaVM* vm);

#endif
