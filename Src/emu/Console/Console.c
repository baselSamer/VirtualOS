#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "../Core/Logger.h"

static int g_console_output_passthrough = 1;

/* Toggles whether printToConsole outputs to stdout (1) or logs to the emulator (0). */
void setConsoleOutputPassthrough(int enabled) {
    g_console_output_passthrough = enabled ? 1 : 0;
}

/* Prints formatted output to either stdout or the emulator log depending on passthrough state. */
void printToConsole(const char* format, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    if (g_console_output_passthrough) {
        printf("%s\n", buffer);
    } else {
        emulatorLog("%s", buffer);
    }
}

/* Bypasses the emulator log entirely and prints directly to standard output. */
void printToMainConsole(const char* format, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    printf("%s\n", buffer);
}

/* Prints an input prompt format directly to standard output. */
void printInputPrompt(const char* format, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    printf("%s\n", buffer);
}

/* Reads formatted input securely from standard input using vscanf. */
void readFromConsole(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vscanf(format, args);
    va_end(args);
}

/* Reads dynamically sized standard input strings, omitting leading whitespace. */
static int read_dynamic_string(char** out_str) {
    int ch;
    size_t cap = 32;
    size_t len = 0;
    char* buf = (char*)malloc(cap);

    if (buf == NULL) {
        return -1;
    }

    do {
        ch = getchar();
        if (ch == EOF) {
            free(buf);
            return -1;
        }
    } while (isspace((unsigned char)ch));

    while (ch != EOF && !isspace((unsigned char)ch)) {
        if (len + 1 >= cap) {
            size_t new_cap = cap * 2;
            char* next = (char*)realloc(buf, new_cap);
            if (next == NULL) {
                free(buf);
                return -1;
            }
            buf = next;
            cap = new_cap;
        }

        buf[len++] = (char)ch;
        ch = getchar();
    }

    buf[len] = '\0';
    *out_str = buf;
    return 0;
}

/* Reads formatted input and dynamically allocates memory for the result based on the format specifier. */
int readFromConsoleAlloc(const char* format, void** out_ptr) {
    size_t fmt_len;
    char spec;
    char* lmark;
    void* mem;

    if (format == NULL || out_ptr == NULL) {
        return -1;
    }

    *out_ptr = NULL;
    fmt_len = strlen(format);
    if (fmt_len < 2 || format[0] != '%') {
        return -1;
    }

    spec = format[fmt_len - 1];
    lmark = strchr(format, 'l');

    if (spec == 's') {
        char* str = NULL;
        if (read_dynamic_string(&str) != 0) {
            return -1;
        }
        *out_ptr = str;
        return 0;
    }

    if (spec == 'd' || spec == 'i') {
        if (lmark != NULL && lmark[1] == 'l') {
            long long* v = (long long*)malloc(sizeof(long long));
            if (v == NULL || scanf(format, v) != 1) {
                free(v);
                return -1;
            }
            *out_ptr = v;
            return 0;
        }

        if (lmark != NULL) {
            long* v = (long*)malloc(sizeof(long));
            if (v == NULL || scanf(format, v) != 1) {
                free(v);
                return -1;
            }
            *out_ptr = v;
            return 0;
        }

        mem = malloc(sizeof(int));
        if (mem == NULL || scanf(format, (int*)mem) != 1) {
            free(mem);
            return -1;
        }
        *out_ptr = mem;
        return 0;
    }

    if (spec == 'u') {
        mem = malloc(sizeof(unsigned int));
        if (mem == NULL || scanf(format, (unsigned int*)mem) != 1) {
            free(mem);
            return -1;
        }
        *out_ptr = mem;
        return 0;
    }

    if (spec == 'f' || spec == 'g' || spec == 'e') {
        if (lmark != NULL) {
            double* v = (double*)malloc(sizeof(double));
            if (v == NULL || scanf(format, v) != 1) {
                free(v);
                return -1;
            }
            *out_ptr = v;
            return 0;
        }

        mem = malloc(sizeof(float));
        if (mem == NULL || scanf(format, (float*)mem) != 1) {
            free(mem);
            return -1;
        }
        *out_ptr = mem;
        return 0;
    }

    if (spec == 'c') {
        mem = malloc(sizeof(char));
        if (mem == NULL || scanf(format, (char*)mem) != 1) {
            free(mem);
            return -1;
        }
        *out_ptr = mem;
        return 0;
    }

    return -1;
}