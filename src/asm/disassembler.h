#ifndef RISA_DISASSEMBLER_H_GUARD
#define RISA_DISASSEMBLER_H_GUARD

#include "../chunk/chunk.h"

void   disassembler_process_chunk(Chunk* chunk);
size_t disassembler_process_instruction(Chunk* chunk, size_t offset);

#endif
