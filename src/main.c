#include "common/headers.h"
#include "common/logging.h"

#include "chunk/chunk.h"
#include "chunk/bytecode.h"
#include "debug/disassembler.h"
#include "memory/mem.h"
#include "vm/vm.h"

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
    VM* vm = vm_create();
    Chunk* chunk = chunk_create();

    chunk_write_constant(chunk, 3.5);
    chunk_write_constant(chunk, 4.7);
    chunk_write_constant(chunk, 5.1);

    chunk_write(chunk, OP_CNST, 1);
    chunk_write(chunk, 1, 1);
    chunk_write(chunk, OP_NEG, 1);

    chunk_write(chunk, OP_CNST, 2);
    chunk_write(chunk, 0, 2);
    chunk_write(chunk, OP_ADD, 2);

    chunk_write(chunk, OP_CNST, 3);
    chunk_write(chunk, 2, 3);
    chunk_write(chunk, OP_MUL, 3);

    chunk_write(chunk, OP_RET, 4);

    //debug_disassemble_chunk(chunk);

    vm_execute(vm, chunk);

    chunk_delete(chunk);
    vm_free(vm);
    mem_free(chunk);
    mem_free(vm);
}

void run_file(const char* path) {

}