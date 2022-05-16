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

uint64_t offset_current = 0;

byte  *all_bytes;
size_t all_bytes_cap;
size_t all_bytes_len;

size_t        byte_sections_cap;
size_t        byte_sections_len;
byte_section *byte_sections;

#define arr_init(arr, cap, len, t, cap_0)                     \
    arr = malloc(sizeof(t) * cap_0);                          \
    if (!arr) {                                               \
        errorf("Failed to initialize internal parser array"); \
        exit(1);                                              \
    }                                                         \
    cap = cap_0;                                              \
    len = 0;

#define arr_add(val, arr, cap, len, t, inc)                       \
    if (len + 1 >= cap) {                                         \
        cap += (inc);                                             \
        arr = realloc(arr, sizeof(t) * cap);                      \
        if (!arr) {                                               \
            errorf("Failed to reallocate internal parser array"); \
            exit(1);                                              \
        }                                                         \
    }                                                             \
    arr[len] = (val);                                             \
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

void add_section(size_t bytes_start, size_t bytes_end) {
    byte_section b;
    b.address_start = offset_current;
    b.bytes_start   = bytes_start;
    b.bytes_length  = bytes_end - bytes_start;

    offset_current += b.bytes_length;

    arr_add(b, byte_sections, byte_sections_cap, byte_sections_len, byte_section, 256);
}

byte char2byte(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    else if (c >= 'a' && c <= 'f') {
        return c - 'a' + 0xA;
    }
    else if (c >= 'A' && c <= 'F') {
        return c - 'A' + 0xA;
    }
    return 0xFF;
}

void parse_line_start();
void parse_skip_whitespace_and_comments();
void parse_skip_inline_whitespace();
void parse_comment();
void parse_BYTES();
void parse_ASCII();
void parse_OFFSET();

addr6502 parse_addr6502_no_ws();

addr6502 parse_addr6502_no_ws() {
    addr6502 value = 0;
    accept_char('$');
    long abs_before = pos_abs;
    while (true) {
        char c = peek();
        byte b = char2byte(c);
        if (b < 0x10) { // valid
            read();
            value <<= 4;
            value += b;
        }
        else {
            break;
        }
    }

    if (abs_before == pos_abs) {
        errorf("Expected at least one hex digit after $");
        exit(1);
    }
    if (abs_before + 4 < pos_abs) {
        errorf("Addresses should be 4-char (16-bit) values");
        exit(1);
    }
    return value;
}

void parse_start() {
    arr_init(all_bytes, all_bytes_cap, all_bytes_len, byte, 4096);
    arr_init(byte_sections, byte_sections_cap, byte_sections_len, byte_section, 256);

    buff_len = 0;
    buff_pos = 0;
    pos_abs  = 0;
    col      = 0;
    line     = 1;

    parse_skip_whitespace_and_comments();
    while (!(buff_pos + 1 >= buff_len && feof(file_asm))) {
        parse_line_start();
    }

    col  = -2;
    line = -2;
#if DEBUG
    debugf("Ended parse.\n");
    debugf("Byte Sections: %li", byte_sections_len);
    for (int i = 0; i < byte_sections_len; i++) {
        byte_section b = byte_sections[i];
        fprintf(stderr, "[DEBUG]   * $%04x: ", b.address_start);
        byte *bs = all_bytes + b.bytes_start;
#define NBYTES_PRINTED_PER_LINE 0x8
        int ascii_offset = 0;
        for (int i = 0; i < b.bytes_length; i++) {
            if (i != 0 && i % NBYTES_PRINTED_PER_LINE == 0) {
                fprintf(stderr, "  %.*s\n", NBYTES_PRINTED_PER_LINE, bs + ascii_offset);
                fprintf(stderr, "            $%04x: ", b.address_start + i);
                ascii_offset += NBYTES_PRINTED_PER_LINE;
            }
            fprintf(stderr, "%02x ", bs[i]);
        }
        int mod = (b.bytes_length % NBYTES_PRINTED_PER_LINE);
        fprintf(stderr, "%.*s  %.*s\n", 3 * (NBYTES_PRINTED_PER_LINE - mod), "                         ", mod, bs + ascii_offset);
        // if ()
        // ,
        //     b.bytes_length,
        //     all_bytes + b.bytes_start
    }

#endif
}

void parse_line_start() {
    char p = peek();
    switch (p) {
        case 'A':
        case 'a':
            parse_ASCII();
            break;
        case 'B':
        case 'b':
            parse_BYTES();
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

void parse_skip_whitespace_and_comments() {
    do {
        char p = peek();
        if (p == '#') {
            parse_comment();
        }
        else if (isspace(p)) {
            read();
        }
        else {
            break;
        }
    } while (true);
}

void parse_comment() {
    tracef("Comment");
    accept_char('#');
    char p = peek();
    while (p != EOF && p != '\n') {
        read();
        p = peek();
    }
    parse_skip_whitespace_and_comments();
}

void parse_ASCII() {
    accept_string("ASCII");
    parse_skip_inline_whitespace();
    accept_char('"');

    size_t start = all_bytes_len;

    while (true) {
        char c = read();
        if (c == '\\') {
            c = read();
            switch (c) {
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

    add_section(start, all_bytes_len);
    parse_skip_whitespace_and_comments();
}

void parse_OFFSET() {
    accept_string("OFFSET");
    parse_skip_inline_whitespace();

    offset_current = parse_addr6502_no_ws();

    parse_skip_whitespace_and_comments();
}

void parse_BYTES() {
    accept_string("BYTES");
    parse_skip_inline_whitespace();
    accept_char('$');

    size_t prev_buff_size_bytes = 0;
    byte  *prev_buff            = NULL;
    int    buff_pos_start = buff_pos + 1;// next char
    while (true) {
        if (buff_pos + 1 >= buff_len) { // about to peek into a new buffer
            debugf("Copying old buffer before new buffer read");
            size_t buff_inc = sizeof(byte) * (buff_len - buff_pos_start);
            if (prev_buff) {
                prev_buff = realloc(prev_buff, prev_buff_size_bytes + buff_inc);
                if (!prev_buff) {
                    errorf("Failed to reallocate internal parser array");
                    exit(1);
                }
            }
            else {
                prev_buff = malloc(buff_inc);
            }
            memcpy(prev_buff + prev_buff_size_bytes, buffer + buff_pos_start, buff_inc);
            prev_buff_size_bytes += buff_inc;
            buff_pos_start = 0;
        }
        if (!isxdigit(peek())) {
            break;
        }
        read();
    }

    byte  *added_bytes     = NULL;
    size_t added_bytes_len = -1;
    if (!prev_buff) {
        added_bytes     = buffer + buff_pos_start;
        added_bytes_len = buff_pos - buff_pos_start;
    }
    else {
        if (buff_pos > 0) {
            size_t buff_inc = sizeof(byte) * (buff_pos + 1);
            prev_buff       = realloc(prev_buff, prev_buff_size_bytes + buff_inc);
            if (!prev_buff) {
                errorf("Failed to reallocate internal parser array");
                exit(1);
            }
            memcpy(prev_buff + prev_buff_size_bytes, buffer, buff_inc);
            prev_buff_size_bytes += buff_inc;
        }
        added_bytes     = prev_buff;
        added_bytes_len = prev_buff_size_bytes / sizeof(byte);
    }

    if (added_bytes_len < 0 || !added_bytes) {
        errorf("Something went wrong...");
        exit(-1);
    }

    if (added_bytes_len == 0) {
        errorf("Expected at least one hex digit after $");
        exit(1);
    }

    size_t bytes_start = all_bytes_len;
    if (added_bytes_len % 2 == 1) {
        debugf("odd %i", added_bytes_len);
        arr_add(char2byte(added_bytes[0]), all_bytes, all_bytes_cap, all_bytes_len, byte, 1024);
        added_bytes = added_bytes + 1;
        added_bytes_len--;
    }

    for (int i = 0; i < added_bytes_len; i += 2) {
        byte b = (char2byte(added_bytes[i]) << 4) | char2byte(added_bytes[i + 1]);
        arr_add(b, all_bytes, all_bytes_cap, all_bytes_len, byte, 1024);
    }

    if (prev_buff) {
        free(prev_buff);
    }

    add_section(bytes_start, all_bytes_len);

    parse_skip_whitespace_and_comments();
}
