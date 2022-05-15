#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "headers/log.h"
#include "headers/parse.h"

char *file_name_asm;
char *file_name_bin;

bool parse_switches(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        if (i != argc - 1) {
            errorf("Unknown switch %s", argv[i]);
            return false;
        }
        file_name_asm = argv[i];
        if (!file_name_bin) {
            size_t len_path = strlen(argv[i]);
            file_name_bin   = malloc(sizeof(char) * (len_path + 5));
            strncpy(file_name_bin, argv[i], len_path);
            strncpy(file_name_bin + len_path, ".bin", 5);
        }
    }

    return file_name_asm != NULL;
}

int main(int argc, char **argv) {
#ifdef DEBUG
    debugf("Debug Build");
#endif

    if (!parse_switches(argc, argv)) {
        return 1;
    }

    debugf("ASM File: %s", file_name_asm);
    debugf("BIN File: %s", file_name_bin);

    if (!(file_asm = fopen(file_name_asm, "r"))) {
        errorf("Failed to open asm file %s.", file_name_asm);
    }
    if (!(file_bin = fopen(file_name_bin, "w"))) {
        errorf("Failed to open asm file %s.", file_name_bin);
    }

    parse_start();

    fclose(file_bin);
    fclose(file_asm);
    return 0;
}
