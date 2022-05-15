#ifndef PARSE_H
#define PARSE_H

#include "log.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <ctype.h>
#include <stdint.h>

extern FILE *file_asm;
extern FILE *file_bin;

bool get_next_buffer();
char read();
char peek();

void parse_start();

typedef uint8_t byte;

typedef struct {
    size_t bytes_start;
    size_t bytes_end;
} unlocated_bytes;

typedef struct {
    unlocated_bytes bytes;
    size_t address_start;
} located_bytes;

#endif