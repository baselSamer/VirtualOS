#ifndef PROJECT_ETHOS_LOGGER_H
#define PROJECT_ETHOS_LOGGER_H

/* Writes a formatted log entry either to an internal GUI array or appends it to a "logs" file. */
void emulatorLog(const char* format, ...);

extern int g_use_gui_logs;
extern char g_gui_logs[40][256];
extern int g_gui_log_count;

#endif