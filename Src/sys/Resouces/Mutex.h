#ifndef MUTEX_H
#define MUTEX_H

#include <stdbool.h>
#include <stddef.h>
#include "../kernal.h"

/* File Mutex Functions (universal single resource) */
/* Returns true if the universal file mutex is currently locked by a process. */
bool FileMutexTaken(kernal_state *state);
/* Returns true if the active process associated with the emulator currently holds the file mutex. */
bool hasFileMutex(kernal_state *state, Emulator *emu);
/* Attempts to acquire the file mutex for the active process. Returns true on success, false if blocked. */
bool acquireFileMutex(kernal_state *state, Emulator *emu);
/* Releases the file mutex if owned by the active process. Returns true on success or false otherwise. */
bool releaseFileMutex(kernal_state *state, Emulator *emu);

/* Console Read Mutex Functions */
/* Returns true if the console read mutex is currently locked by a process. */
bool ConsoleReadMutexTaken(kernal_state *state);
/* Returns true if the active process associated with the emulator currently holds the console read mutex. */
bool hasConsoleReadMutex(kernal_state *state, Emulator *emu);
/* Attempts to acquire the console read mutex for the active process. Returns true on success, false if blocked. */
bool acquireConsoleReadMutex(kernal_state *state, Emulator *emu);
/* Releases the console read mutex if owned by the active process. Returns true on success or false otherwise. */
bool releaseConsoleReadMutex(kernal_state *state, Emulator *emu);

/* Console Write Mutex Functions */
/* Returns true if the console write mutex is currently locked by a process. */
bool ConsoleWriteMutexTaken(kernal_state *state);
/* Returns true if the active process associated with the emulator currently holds the console write mutex. */
bool hasConsoleWriteMutex(kernal_state *state, Emulator *emu);
/* Attempts to acquire the console write mutex for the active process. Returns true on success, false if blocked. */
bool acquireConsoleWriteMutex(kernal_state *state, Emulator *emu);
/* Releases the console write mutex if owned by the active process. Returns true on success or false otherwise. */
bool releaseConsoleWriteMutex(kernal_state *state, Emulator *emu);

#endif /* MUTEX_H */