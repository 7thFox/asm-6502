#include "headers/parse.h"

FILE *file_asm = NULL;
FILE *file_bin = NULL;

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

// TODO JOSH: Reconfigure buffer to alway contain the next 2/4k characters
// basically having 2 buffers and swapping them around as needed
// really you'd just have 1 buffer with 2 2k halfs. after reaching the
// end of one (and into the other) we'll fread the other half. Then you can
// wrap the index so arr[0] == arr[BUFF_LENGTH]. This also will simplify the
// BYTES part; we just can't allow > 2k sections because we can't guarantee
// it's all still in memory

#define BUFF_SIZE     4096
#define BUFF_BOUNDARY (BUFF_SIZE / 2)
// we read the next half of the buffer as soon as we enter the next boundary.
// Worst case we're at the end of this boundary and have the full 2048 bytes
// to seek
bool is_eof = false;
char cyclic_buffer[BUFF_SIZE];
long cyclic_buffer_pos = -1;
long cyclic_buffer_len = -1;
long pos_abs           = -1;
long col               = -1;
long line              = -1;

void setup_cyclic_buffer() {
    line = 1;
    col  = 0;

    cyclic_buffer_len = fread(cyclic_buffer, sizeof(char), BUFF_BOUNDARY, file_asm); // first read will read-behind the other 2k
}

char read() {
    cyclic_buffer_pos++;
    if (is_eof && cyclic_buffer_pos >= cyclic_buffer_len) {
        return EOF;
    }

    int read_pos_abs = cyclic_buffer_pos % BUFF_SIZE;
    if (!is_eof && read_pos_abs % BUFF_BOUNDARY == 0) {
        size_t readbehind_offset = (read_pos_abs + BUFF_BOUNDARY) % BUFF_SIZE;
        tracef("Reading to cyclic buffer to [%i]", readbehind_offset);
        size_t nread = fread(cyclic_buffer + readbehind_offset, sizeof(char), BUFF_BOUNDARY, file_asm);
        cyclic_buffer_len += nread;
        is_eof = nread < BUFF_BOUNDARY;
    }

    char c = cyclic_buffer[read_pos_abs];
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
    long peek_pos_abs = (cyclic_buffer_pos + 1) % BUFF_SIZE;
    return cyclic_buffer[peek_pos_abs];
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

    setup_cyclic_buffer();

    parse_skip_whitespace_and_comments();
    while (!(cyclic_buffer_pos + 1 >= cyclic_buffer_len && feof(file_asm))) {
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

    // we can peek up to 2k bytes before needing to read the next buffer
    // worst-case (next byte is on boundary)
    int seek_offset = 1;
    while (seek_offset < BUFF_BOUNDARY && isxdigit(cyclic_buffer[(cyclic_buffer_pos + seek_offset) % BUFF_SIZE])) {
        seek_offset++;
    }

    if (seek_offset >= BUFF_BOUNDARY) {
        errorf("BYTES exceeds the maximum length of %i characters (%i bytes). Break into smaller segments", BUFF_BOUNDARY, BUFF_BOUNDARY / 2);
        exit(1);
    }

    size_t bytes_start = all_bytes_len;

    if (seek_offset % 2 == 1) {
        arr_add(char2byte(read()), all_bytes, all_bytes_cap, all_bytes_len, byte, 1024);
        seek_offset--;
    }

    for (int i = 0; i < seek_offset; i += 2) {
        byte b = char2byte(read()) << 4;
        b |= char2byte(read());
        arr_add(b, all_bytes, all_bytes_cap, all_bytes_len, byte, 1024);
    }

    add_section(bytes_start, all_bytes_len);

    parse_skip_whitespace_and_comments();
}
