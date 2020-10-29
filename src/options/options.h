#ifndef RISA_OPTIONS_H_GUARD
#define RISA_OPTIONS_H_GUARD

#include "../common/headers.h"

typedef struct {
    bool replMode; // COMPILER: Whether or not to store the lastRed value into the ACC reg.
} Options;

#endif
