#include "Logger.h"

#include <stdio.h>
#include <stdarg.h>

void emulatorLog(const char* format, ...) {
    FILE* log_file;
    va_list args;

    if (format == NULL) {
        return;
    }

    log_file = fopen("logs", "a");
    if (log_file == NULL) {
        return;
    }

    va_start(args, format);
    vfprintf(log_file, format, args);
    va_end(args);

    fputc('\n', log_file);
    fclose(log_file);
}