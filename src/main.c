#include "common/headers.h"
#include "common/logging.h"

#include "risa.h"
#include "memory/mem.h"

#include <stdio.h>
#include <stdlib.h>

void run_repl();
void run_file(const char* path);

int main(int argc, char** argv) {
    if(argc == 1)
        run_repl();
    else if(argc == 2)
        run_file(argv[1]);
    else TERMINATE(64, "Invalid arguments");
}

void run_repl() {
    char line[256];

    while(1) {
        PRINT("> ");

        if(!fgets(line, sizeof(line), stdin)) {
            PRINT("\n");
            break;
        }

        risa_interpret_string(line);
    }
}

void run_file(const char* path) {
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

    RisaInterpretStatus status = risa_interpret_string(data);

    MEM_FREE(data);

    if(status == RISA_INTERPRET_OK)
        exit(0);
    if(status == RISA_INTERPRET_COMPILE_ERROR)
        exit(1);
    if(status == RISA_INTERPRET_EXECUTE_ERROR)
        exit(1);
}