#ifndef MUTEX_H
#define MUTEX_H

#include <stdbool.h>
#include <stddef.h>
#include "../kernal.h"


/* File Mutex Functions */
bool fileMutexTaken(kernal_state *state, const char *path);
bool hasFileMutex(kernal_state *state, const char *path, Emulator *emu);
bool acquireFileMutex(kernal_state *state, const char *path, Emulator *emu);
bool releaseFileMutex(kernal_state *state, const char *path, Emulator *emu);

/* Console Read Mutex Functions */
bool ConsoleReadMutexTaken(kernal_state *state);
bool hasConsoleReadMutex(kernal_state *state, Emulator *emu);
bool acquireConsoleReadMutex(kernal_state *state, Emulator *emu);
bool releaseConsoleReadMutex(kernal_state *state, Emulator *emu);

/* Console Write Mutex Functions */
bool ConsoleWriteMutexTaken(kernal_state *state);
bool hasConsoleWriteMutex(kernal_state *state, Emulator *emu);
bool acquireConsoleWriteMutex(kernal_state *state, Emulator *emu);
bool releaseConsoleWriteMutex(kernal_state *state, Emulator *emu);

#endif /* MUTEX_H */