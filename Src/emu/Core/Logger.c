#include "Logger.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

int g_use_gui_logs = 0;
char g_gui_logs[40][256];
int g_gui_log_count = 0;

/* Writes a formatted log entry either to an internal GUI array or appends it to a "logs" file. */
void emulatorLog(const char* format, ...) {
    FILE* log_file;
    va_list args;

    if (format == NULL) {
        return;
    }
    
    if (g_use_gui_logs) {
        char buffer[256];
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);
        
        if (g_gui_log_count < 40) {
            strcpy(g_gui_logs[g_gui_log_count++], buffer);
        } else {
            for (int i = 1; i < 40; i++) strcpy(g_gui_logs[i-1], g_gui_logs[i]);
            strcpy(g_gui_logs[39], buffer);
        }
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