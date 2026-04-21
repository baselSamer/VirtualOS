#ifndef PROJECT_ETHOS_LOGGER_H
#define PROJECT_ETHOS_LOGGER_H

void emulatorLog(const char* format, ...);

extern int g_use_gui_logs;
extern char g_gui_logs[40][256];
extern int g_gui_log_count;

#endif