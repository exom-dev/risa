#include "common/headers.h"
#include "common/logging.h"

#include "risa.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void run_repl();
void run_file(const char* path);

VM create_vm();

void print_info();

int main(int argc, char** argv) {
    if(argc == 1)
        run_repl();
    else if(argc == 2)
        run_file(argv[1]);
    else TERMINATE(64, "Invalid arguments");
}

Value print(void* vm, uint8_t argc, Value* args) {
    value_print(args[0]);
    return NULL_VALUE;
}

VM create_vm() {
    VM vm;
    vm_init(&vm);

    DenseNative* native = dense_native_create(print);
    DenseString* string = map_find(&vm.strings, "print", 5, map_hash("print", 5));

    if(string == NULL) {
        string = dense_string_from("print", 5);
        vm_register_string(&vm, string);
        vm_register_dense(&vm, (DenseValue*) string);
    }

    map_set(&vm.globals, string, DENSE_VALUE(native));
    vm_register_dense(&vm, (DenseValue*) native);

    return vm;
}

void run_repl() {
    print_info();

    VM vm = create_vm();

    vm.options.replMode = true;

    char line[1024];

    while(1) {
        PRINT("#>");

        if(!fgets(line, sizeof(line) - 1, stdin)) {
            PRINT("\n");
            break;
        }

        size_t length = strlen(line);

        if(line[length - 1] == '\n') {
            --length;
            line[length] = '\0';
        }

        if(strcmp(line, "exit") == 0)
            exit(0);

        if(line[length - 1] != ';') {
            line[length] = ';';
            line[length + 1] = '\0';
        }

        RisaInterpretStatus status = risa_interpret_string(&vm, line);

        if(status == RISA_INTERPRET_OK && vm.acc.type != VAL_NULL)
            value_print(vm.acc);
        PRINT("\n");
    }
}

void run_file(const char* path) {
    VM vm = create_vm();

    FILE* file = fopen(path, "rb");

    if(file == NULL)
        TERMINATE(74, "Cannot open file '%s'\n", path);

    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char* data = (char*) MEM_ALLOC(size + 1);

    if(fread(data, sizeof(char), size, file) < size) {
        fclose(file);
        MEM_FREE(data);
        TERMINATE(74, "Cannot read file '%s'\n", path);
    }

    data[size] = '\0';

    fclose(file);

    RisaInterpretStatus status = risa_interpret_string(&vm, data);

    MEM_FREE(data);

    if(status == RISA_INTERPRET_OK)
        exit(0);
    if(status == RISA_INTERPRET_COMPILE_ERROR)
        exit(1);
    if(status == RISA_INTERPRET_EXECUTE_ERROR)
        exit(1);
}

void print_info() {
    PRINT("Risa v%s '%s'\n", RISA_VERSION, RISA_CODENAME);
    PRINT("(c) 2020 The Exom Developers (exom.dev)\n\n");

    PRINT("     _____________________      _______\n");
    PRINT("    |#####################\\    /######/\n");
    PRINT("    |######################\\  /######/\n");
    PRINT("    |#######################\\/######/\n");
    PRINT("    |#####|  _______   \\###########/\n");
    PRINT("    |#####| |######/    \\#########/\n");
    PRINT("    |#####| |#####/      \\#######/\n");
    PRINT("    |#####| |####/       /#######\\\n");
    PRINT("    |#####|             /#########\\\n");
    PRINT("    |#####|____________/###########\\\n");
    PRINT("    |#######################/\\######\\\n");
    PRINT("    |######################/  \\######\\\n");
    PRINT("    |#####################/    \\######\\\n\n");
}