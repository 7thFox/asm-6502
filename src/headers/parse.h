#ifndef PARSE_H
#define PARSE_H

#include "log.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if DEBUG
#include <time.h>
#endif

extern FILE *file_asm;
extern FILE *file_bin;

bool get_next_buffer();
char read();
char peek();

void parse_start();

typedef uint8_t  byte;
typedef uint16_t addr6502;

typedef struct {
    size_t address_start;
    size_t bytes_start;
    size_t bytes_length;
} byte_section;

// this_is_the_max_label_length___________________________________:
#define MAX_LABEL_LENGTH 64
typedef struct {
    const char *label;
    addr6502    offset;
} label_offset;

#endif