#ifndef RISA_DISASSEMBLER_H_GUARD
#define RISA_DISASSEMBLER_H_GUARD

#include "../io/io.h"
#include "../chunk/chunk.h"
#include "../def/types.h"

typedef struct {
    RisaIO io;
    Chunk* chunk;
    size_t offset;
} RisaDisassembler;

void disassembler_init(RisaDisassembler* disassembler);
void disassembler_load(RisaDisassembler* disassembler, Chunk* chunk);
void disassembler_run(RisaDisassembler* disassembler);
void disassembler_reset(RisaDisassembler* disassembler);

#endif
