#include "risa.h"

#include "def/types.h"
#include "io/log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TERMINATE(io, code, fmt, ...) \
    do {\
        RISA_ERROR(io, fmt, ##__VA_ARGS__ ); \
        exit(code); \
    } while(0)

void run_repl(RisaIO io);
void run_file(RisaIO io, const char* path);

VM create_vm();

void print_info(RisaIO io);

int main(int argc, char** argv) {
    RisaIO io;
    risa_io_init(&io);

    if(argc == 1)
        run_repl(io);
    else if(argc == 2)
        run_file(io, argv[1]);
    else TERMINATE(io, 64, "Invalid arguments");
}

VM create_vm() {
    VM vm;
    vm_init(&vm);
    std_register_core(&vm);
    std_register_io(&vm);

    //vm_global_set_native(&vm, "print", 5, print);

    return vm;
}

void run_repl(RisaIO io) {
    print_info(io);

    VM vm = create_vm();

    vm.options.replMode = true;

    char line[1024];

    while(1) {
        RISA_OUT(io, "#>");

        if(!fgets(line, sizeof(line) - 1, stdin)) {
            RISA_IO_STDOUT("\n");
            break;
        }

        size_t length = strlen(line);

        if(line[length - 1] == '\n') {
            --length;
            line[length] = '\0';
        }

        if(strcmp(line, "exit") == 0) {
            vm_delete(&vm);
            exit(0);
        }

        if(line[length - 1] != ';') {
            line[length] = ';';
            line[length + 1] = '\0';
        }

        RisaInterpretStatus status = risa_interpret_string(&vm, line);

        if(status == RISA_INTERPRET_OK && vm.acc.type != VAL_NULL)
            value_print(&vm.io, vm.acc);
        RISA_OUT(io, "\n");
    }
}

void run_file(RisaIO io, const char* path) {
    VM vm = create_vm();

    FILE* file = fopen(path, "rb");

    if(file == NULL)
        TERMINATE(io, 74, "Cannot open file '%s'\n", path);

    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char* data = (char*) RISA_MEM_ALLOC(size + 1);

    if(fread(data, sizeof(char), size, file) < size) {
        fclose(file);
        RISA_MEM_FREE(data);
        TERMINATE(io, 74, "Cannot read file '%s'\n", path);
    }

    data[size] = '\0';

    fclose(file);

    RisaInterpretStatus status = risa_interpret_string(&vm, data);

    RISA_MEM_FREE(data);

    vm_delete(&vm);

    #ifdef DEBUG_SHOW_HEAP_SIZE
        RISA_OUT(vm.io, "\n\nHeap size: %zu\n", vm.heapSize);
    #endif

    if(status == RISA_INTERPRET_OK)
        exit(0);
    if(status == RISA_INTERPRET_COMPILE_ERROR)
        exit(1);
    if(status == RISA_INTERPRET_EXECUTE_ERROR)
        exit(1);
}

void print_info(RisaIO io) {
    RISA_OUT(io, "Risa v%s '%s'\n", RISA_VERSION, RISA_CODENAME);
    RISA_OUT(io, "(c) 2020 The Exom Developers (exom.dev)\n\n");

    RISA_OUT(io, "     _____________________      _______\n");
    RISA_OUT(io, "    |#####################\\    /######/\n");
    RISA_OUT(io, "    |######################\\  /######/\n");
    RISA_OUT(io, "    |#######################\\/######/\n");
    RISA_OUT(io, "    |#####|  _______   \\###########/\n");
    RISA_OUT(io, "    |#####| |######/    \\#########/\n");
    RISA_OUT(io, "    |#####| |#####/      \\#######/\n");
    RISA_OUT(io, "    |#####| |####/       /#######\\\n");
    RISA_OUT(io, "    |#####|             /#########\\\n");
    RISA_OUT(io, "    |#####|____________/###########\\\n");
    RISA_OUT(io, "    |#######################/\\######\\\n");
    RISA_OUT(io, "    |######################/  \\######\\\n");
    RISA_OUT(io, "    |#####################/    \\######\\\n\n");
}

#undef TERMINATE