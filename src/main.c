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
void run_args(RisaIO io, int argc, char* argv[]);
void run_file(RisaIO io, const char* path);
void compile_file(RisaIO io, const char* input, const char* output);

RisaVM create_vm();

void print_info(RisaIO io);

int main(int argc, char* argv[]) {
    RisaIO io;
    risa_io_init(&io);

    if(argc == 1) {
        run_repl(io);
    } else run_args(io, argc, argv);
}

RisaVM create_vm() {
    RisaVM vm;
    risa_vm_init(&vm);

    risa_std_register_core(&vm);
    risa_std_register_io(&vm);
    risa_std_register_string(&vm);
    risa_std_register_math(&vm);
    risa_std_register_reflect(&vm);
    risa_std_register_debug(&vm);

    return vm;
}

void run_repl(RisaIO io) {
    print_info(io);

    RisaVM vm = create_vm();

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
            risa_vm_delete(&vm);
            exit(0);
        }

        if(line[length - 1] != ';') {
            line[length] = ';';
            line[length + 1] = '\0';
        }

        RisaInterpretStatus status = risa_interpret_string(&vm, line);

        if(status == RISA_INTERPRET_OK && vm.acc.type != RISA_VAL_NULL)
            risa_value_print(&vm.io, vm.acc);

        RISA_OUT(io, "\n");
    }
}

void run_args(RisaIO io, int argc, char* argv[]) {
    if(0 == strcmp(argv[1], "-c")) {
        if(argc < 4) {
            TERMINATE(io, 64, "Invalid arguments");
        }

        compile_file(io, argv[2], argv[3]);
    } else {
        run_file(io, argv[1]);
    }
}

void run_file(RisaIO io, const char* path) {
    RisaVM vm = create_vm();

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

    if(size >= sizeof(RISA_CLUSTER_MAGIC) - 1 && memcmp(data, RISA_CLUSTER_MAGIC, sizeof(RISA_CLUSTER_MAGIC) - 1) == 0) {
        RisaClusterDeserializer deserializer;

        risa_cluster_deserializer_init(&deserializer);
        risa_cluster_deserializer_target(&deserializer, &vm);

        RisaClusterDeserializationStatus result = risa_cluster_deserializer_deserialize(&deserializer, (uint8_t*) data, size);

        RISA_MEM_FREE(data);

        if(result != RISA_DESERIALIZATION_OK) {
            RISA_ERROR(vm.io, "error: cannot load compiled script (file is corrupted)\n");
            exit(1);
        }

        RisaExecuteStatus status = risa_execute_cluster(&vm, deserializer.output);

        risa_vm_delete(&vm);
        risa_cluster_deserializer_delete(&deserializer);

        #ifdef DEBUG_SHOW_HEAP_SIZE
                RISA_OUT(vm.io, "\n\nHeap size: %zu\n", vm.heapSize);
        #endif

        if(status != RISA_EXECUTE_OK) {
            exit(1);
        }
    } else {
        RisaInterpretStatus status = risa_interpret_string(&vm, data);

        RISA_MEM_FREE(data);

        risa_vm_delete(&vm);

        #ifdef DEBUG_SHOW_HEAP_SIZE
                RISA_OUT(vm.io, "\n\nHeap size: %zu\n", vm.heapSize);
        #endif

        if(status != RISA_INTERPRET_OK) {
            exit(1);
        }
    }
}

void compile_file(RisaIO io, const char* input, const char* output) {
    FILE* file = fopen(input, "rb");

    if(file == NULL)
        TERMINATE(io, 74, "Cannot open file '%s'\n", input);

    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char* data = (char*) RISA_MEM_ALLOC(size + 1);

    if(fread(data, sizeof(char), size, file) < size) {
        fclose(file);
        RISA_MEM_FREE(data);
        TERMINATE(io, 74, "Cannot read file '%s'\n", input);
    }

    data[size] = '\0';

    fclose(file);

    RisaCompiler compiler;
    risa_compiler_init(&compiler);

    RisaCompileStatus status = risa_compile_string(&compiler, data);

    RISA_MEM_FREE(data);

    if(status == RISA_COMPILER_STATUS_OK) {
        uint32_t compiledSize = 0;
        uint8_t* compiled = risa_serialize_cluster(&compiler.function->cluster, &compiledSize);

        file = fopen(output, "wb");

        if(file == NULL) {
            TERMINATE(io, 75, "Cannot write file '%s'\n", output);
        }

        fwrite(compiled, sizeof(uint8_t), compiledSize, file);
        fclose(file);

        risa_compiler_delete(&compiler);
    } else {
        exit(1);
    }
}

void print_info(RisaIO io) {
    RISA_OUT(io, "Risa v%s '%s'\n", RISA_VERSION_STRING, RISA_VERSION_CODENAME);
    RISA_OUT(io, "(c) 2020-2021 The Exom Developers (exom.dev)\n\n");

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