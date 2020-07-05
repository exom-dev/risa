#include "common/headers.h"
#include "common/logging.h"

#include "chunk/chunk.h"
#include "chunk/bytecode.h"
#include "debug/disassembler.h"
#include "memory/mem.h"

#include <stdio.h>
#include <stdlib.h>

void repl();
void run_file(const char* path);

int main(int argc, char** argv) {
    if(argc == 1)
        repl();
    else if(argc == 2)
        run_file(argv[1]);
    else TERMINATE(64, "Invalid arguments");
}

void repl() {
    Chunk* chunk = chunk_create();

    chunk_write_constant(chunk, 3.5);
    chunk_write_constant(chunk, 4.7);
    chunk_write_constant(chunk, 5.1);

    chunk_write(chunk, OP_CONSTANT, 1);
    chunk_write(chunk, 1, 1);
    chunk_write(chunk, OP_RETURN, 2);

    risa_disassemble_chunk(chunk);

    chunk_delete(chunk);

    mem_free(chunk);
}

void run_file(const char* path) {

}