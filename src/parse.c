#include "headers/parse.h"

FILE *file_asm = NULL;
FILE *file_bin = NULL;

#define BUFF_SIZE 4096
char buffer[BUFF_SIZE];
int  buff_len = -1;
int  buff_pos = -1;
long pos_abs  = -1;
long col      = -1;
long line     = -1;

uint64_t offset_current;
bool     offset_is_set = false;

byte  *all_bytes;
size_t all_bytes_cap;
size_t all_bytes_len;

size_t           unlocated_cap;
size_t           unlocated_len;
unlocated_bytes *unlocated;

size_t         located_cap;
size_t         located_len;
located_bytes *located;

#define arr_init(arr, cap, len, t, cap_0)                     \
    arr = malloc(sizeof(t) * cap_0);                          \
    if (!arr) {                                               \
        errorf("Failed to initialize internal parser array"); \
        exit(1);                                              \
    }                                                         \
    cap = cap_0;                                              \
    len = 0;

#define arr_add(val, arr, cap, len, t, inc)                            \
    if (len + 1 >= cap) {                                         \
        cap += inc;                                               \
        arr = realloc(arr, sizeof(t) * cap);                      \
        if (!arr) {                                               \
            errorf("Failed to reallocate internal parser array"); \
            exit(1);                                              \
        }                                                         \
    }                                                             \
    arr[len] = val;                                               \
    len++;

bool get_next_buffer() {
    buff_len = fread(buffer, sizeof(char), BUFF_SIZE, file_asm);
    tracef("Read in new buffer of %i characters", buff_len);
    if (ferror(file_asm)) {
        errorf("Error reading from file");
        exit(1);
    }
}

char read() {
    buff_pos++;
    if (buff_pos >= buff_len) {
        get_next_buffer();
        if (!buff_len) {
            return EOF;
        }
        buff_pos = 0;
    }

    char c = buffer[buff_pos];
    pos_abs++;
    if (c == '\n') {
        line++;
        col = 1;
    }
    else {
        col++;
    }

    tracef("READ '%c'", c);

    return c;
}

char peek() {
    long peek_pos = buff_pos + 1;
    if (peek_pos >= buff_len) {
        get_next_buffer();
        if (!buff_len) {
            return EOF;
        }
        buff_pos = -1;
        peek_pos = 0;
    }

    tracef("PEEK");

    return buffer[peek_pos];
}

void accept_string(const char *value_upper) {
    for (int i = 0; value_upper[i] != '\0'; i++) {
        char c = read();
        if (toupper(c) != value_upper[i]) {
            errorf("Unexpected character '%c'. Expected '%c' in keyword %s.", c, value_upper[i], value_upper);
            exit(1);
        }
    }
}
void accept_char(char value_upper) {
    char c = read();
    if (toupper(c) != value_upper) {
        errorf("Unexpected character '%c'. Expected '%c'.", c, value_upper);
        exit(1);
    }
}

void parse_line_start();
void parse_skip_whitespace();
void parse_skip_inline_whitespace();
void parse_comment();
void parse_ASCII();
void parse_OFFSET();

uint64_t parse_hex_no_ws();

void add_bytes(unlocated_bytes b) {
    if (offset_is_set) {
        located_bytes bb;
        bb.bytes = b;
        bb.address_start = offset_current;
        arr_add(bb, located, located_cap, located_len, located_bytes, 256);
    } else {
        arr_add(b, unlocated, unlocated_cap, unlocated_len, unlocated_bytes, 256);
    }
}

void parse_start() {
    arr_init(all_bytes, all_bytes_cap, all_bytes_len, byte, 4096);
    arr_init(unlocated, unlocated_cap, unlocated_len, unlocated_bytes, 256);
    arr_init(located, located_cap, located_len, located_bytes, 256);

    buff_len = 0;
    buff_pos = 0;
    pos_abs  = 0;
    col      = 0;
    line     = 1;

    parse_skip_whitespace();
    while (!(buff_pos + 1 >= buff_len && feof(file_asm))) {
        parse_line_start();
    }


    col      = -2;
    line     = -2;
#if DEBUG
    debugf("Ended parse.\n");
    debugf("UNLOCATED byte sections: %li", unlocated_len);
    for (int i = 0; i < unlocated_len; i++) {
        unlocated_bytes b = unlocated[i];
        debugf("     ????: '%.*s'",
               b.bytes_end - b.bytes_start,
               all_bytes + b.bytes_start);
    }

    debugf("LOCATED byte sections: %li", located_len);
    for (int i = 0; i < located_len; i++) {
        located_bytes b = located[i];
        debugf("    $%x: '%.*s'",
               b.address_start,
               b.bytes.bytes_end - b.bytes.bytes_start,
               all_bytes + b.bytes.bytes_start);
    }

#endif

}

void parse_line_start() {
    char p = peek();
    switch (p) {
        case '#':
            parse_comment();
            break;
        case 'A':
        case 'a':
            parse_ASCII();
            break;
        case 'O':
        case 'o':
            parse_OFFSET();
            break;
        default:
            errorf("Unexpected character '%c'.", p);
            exit(1);
    }
}

void parse_skip_inline_whitespace() {
    char p = peek();
    while (p != EOF && p != '\n' && isspace(p)) {
        read();
        p = peek();
    }
}

void parse_skip_whitespace() {
    char p = peek();
    while (p != EOF && isspace(p)) {
        read();
        p = peek();
    }
}

void parse_comment() {
    tracef("Comment");
    accept_char('#');
    char p = peek();
    while (p != EOF && p != '\n') {
        read();
        p = peek();
    }
    parse_skip_whitespace();
}

void parse_ASCII() {
    accept_string("ASCII");
    parse_skip_inline_whitespace();
    accept_char('"');

    unlocated_bytes b;
    b.bytes_start = all_bytes_len;

    while (true) {
        char c = read();
        if (c == '\\') {
            c = read();
            switch (c)
            {
                case '"':
                    arr_add('"', all_bytes, all_bytes_cap, all_bytes_len, byte, 1024);
                    break;
                    // TODO: Other Escapes
                default:
                    errorf("Unknown escape sequence '\\%c'.", c);
                    exit(1);
                    break;
            }
        }
        else if (c == '"') {
            tracef("End of string");
            break;
        }
        else {
            arr_add(c, all_bytes, all_bytes_cap, all_bytes_len, byte, 1024);
        }
    }
    b.bytes_end = all_bytes_len;
    add_bytes(b);
    parse_skip_whitespace();
}

void parse_OFFSET(){
    accept_string("OFFSET");
    parse_skip_inline_whitespace();

    if (toupper(peek()) == 'N') {
        accept_string("NONE");
        offset_is_set = false;
    } else {
        offset_current = parse_hex_no_ws();
        offset_is_set  = true;
    }

    parse_skip_whitespace();
}

uint64_t parse_hex_no_ws() {
    uint64_t value = 0;
    accept_char('$');
    long     abs_before = pos_abs;
    while (true) {
        char c = peek();
        if (c >= '0' && c <= '9') {
            read();
            value <<= 4;
            value += c - '0';
        }
        else if (c >= 'a' && c <= 'f') {
            read();
            value <<= 4;
            value += c - 'a' + 0xA;
        }
        else if (c >= 'A' && c <= 'F') {
            read();
            value <<= 4;
            value += c - 'A' + 0xA;
        } else {
            break;
        }
    }

    if (abs_before == pos_abs) {
        errorf("Expected at least one hex digit after $");
        exit(1);
    }
    return value;
}