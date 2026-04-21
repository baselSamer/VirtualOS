#ifndef PROJECT_ETHOS_CONSOLE_H
#define PROJECT_ETHOS_CONSOLE_H

void printToConsole(const char* format, ...);
void printInputPrompt(const char* format, ...);
void readFromConsole(const char* format, ...);
int readFromConsoleAlloc(const char* format, void** out_ptr);
void setConsoleOutputPassthrough(int enabled);

#endif
