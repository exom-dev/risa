#ifndef RISA_DISASSEMBLER_H_GUARD
#define RISA_DISASSEMBLER_H_GUARD

#include "../chunk/chunk.h"

void debug_disassemble_chunk(Chunk* chunk);
size_t debug_disassemble_instruction(Chunk* chunk, size_t offset);

#endif
