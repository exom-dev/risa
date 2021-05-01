#ifndef RISA_ASSEMBLER_H_GUARD
#define RISA_ASSEMBLER_H_GUARD

#include "parser.h"
#include "lexer.h"

#include "../api.h"
#include "../io/io.h"
#include "../cluster/cluster.h"
#include "../data/map.h"
#include "../def/def.h"
#include "../value/dense.h"

typedef enum {
    RISA_ASM_MODE_DATA,
    RISA_ASM_MODE_CODE
} RisaAsmMode;

typedef struct RisaAssembler {
    RisaIO io;
    struct RisaAssembler* super;

    RisaCluster cluster;

    RisaAsmParser* parser;
    RisaMap* strings;

    RisaMap identifiers;

    bool canSwitchToData;
    RisaAsmMode mode;
} RisaAssembler;

typedef enum {
    RISA_ASM_STATUS_OK,
    RISA_ASM_STATUS_ERROR
} AssemblerStatus;

RISA_API void risa_assembler_init   (RisaAssembler* assembler);
RISA_API void risa_assembler_delete (RisaAssembler* assembler);

RISA_API AssemblerStatus risa_assembler_assemble (RisaAssembler* assembler, const char* str, const char* stoppers);

#endif
