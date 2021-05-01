#ifndef RISA_OPTIONS_H_GUARD
#define RISA_OPTIONS_H_GUARD

#include "../def/types.h"

typedef struct {
    bool replMode; // COMPILER: Whether or not to store the lastReg value into the ACC reg.
} RisaOptions;

#endif
