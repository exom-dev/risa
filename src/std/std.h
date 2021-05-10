#ifndef RISA_STD_H_GUARD
#define RISA_STD_H_GUARD

#include "../api.h"
#include "../vm/vm.h"

RISA_API void risa_std_register_core    (RisaVM* vm);
RISA_API void risa_std_register_io      (RisaVM* vm);
RISA_API void risa_std_register_string  (RisaVM* vm);
RISA_API void risa_std_register_math    (RisaVM* vm);
RISA_API void risa_std_register_reflect (RisaVM* vm);
RISA_API void risa_std_register_debug   (RisaVM* vm);

#endif
