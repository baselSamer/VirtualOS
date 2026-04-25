#ifndef PROJECT_ETHOS_CONSOLE_H
#define PROJECT_ETHOS_CONSOLE_H

/* Prints formatted output to either stdout or the emulator log depending on passthrough state. */
void printToConsole(const char* format, ...);
/* Bypasses the emulator log entirely and prints directly to standard output. */
void printToMainConsole(const char* format, ...);
/* Prints an input prompt format directly to standard output. */
void printInputPrompt(const char* format, ...);
/* Reads formatted input securely from standard input using vscanf. */
void readFromConsole(const char* format, ...);
/* Reads formatted input and dynamically allocates memory for the result based on the format specifier. */
int readFromConsoleAlloc(const char* format, void** out_ptr);
/* Toggles whether printToConsole outputs to stdout (1) or logs to the emulator (0). */
void setConsoleOutputPassthrough(int enabled);

#endif
