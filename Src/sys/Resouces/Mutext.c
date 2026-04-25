#include "Mutex.h"
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include "../../emu/Core/Logger.h"

/* ===== File Mutex (Universal Single Resource) ===== */

/* Returns true if the universal file mutex is currently locked by a process. */
bool FileMutexTaken(kernal_state *state) {
    Mutex *mutex = state->mutexes;
    return mutex->File != -1;
}

/* Returns true if the active process associated with the emulator currently holds the file mutex. */
bool hasFileMutex(kernal_state *state, Emulator *emu) {
    Mutex *mutex = state->mutexes;
    return mutex->File == getActivePCB(emu)->ProcessID;
}

/* Attempts to acquire the file mutex for the active process. Returns true on success, false if blocked. */
bool acquireFileMutex(kernal_state *state, Emulator *emu) {
    Mutex *mutex = state->mutexes;
    int pid = getActivePCB(emu)->ProcessID;
    if (mutex->File == pid) {
        emulatorLog("[MUTEX] acquireFileMutex: pid=%d already owns", pid);
        return true; // Already has mutex
    }
    if (FileMutexTaken(state)) {
        emulatorLog("[MUTEX] acquireFileMutex: pid=%d blocked (held by pid=%d)", pid, mutex->File);
        return false; // Mutex is taken by another process
    }
    mutex->File = pid;
    emulatorLog("[MUTEX] acquireFileMutex: pid=%d acquired", pid);
    return true;
}

/* Releases the file mutex if owned by the active process. Returns true on success or false otherwise. */
bool releaseFileMutex(kernal_state *state, Emulator *emu) {
    Mutex *mutex = state->mutexes;
    int pid = getActivePCB(emu)->ProcessID;
    if (mutex->File == pid) {
        mutex->File = -1;
        emulatorLog("[MUTEX] releaseFileMutex: pid=%d released", pid);
        return true;
    }
    emulatorLog("[MUTEX] releaseFileMutex: pid=%d failed (held by pid=%d)", pid, mutex->File);
    return false; // Mutex not held by this process
}

/* ===== Console Read Mutex ===== */

/* Returns true if the console read mutex is currently locked by a process. */
bool ConsoleReadMutexTaken(kernal_state *state) {
    Mutex *mutex = state->mutexes;
    return mutex->ConsoleRead != -1;
}

/* Attempts to acquire the console read mutex for the active process. Returns true on success, false if blocked. */
bool acquireConsoleReadMutex(kernal_state *state, Emulator *emu) {
    Mutex *mutex = state->mutexes;
    if (mutex->ConsoleRead == getActivePCB(emu)->ProcessID) {
        emulatorLog("[MUTEX] acquireConsoleReadMutex: pid=%d already owns", getActivePCB(emu)->ProcessID);
        return true;
    }
    if (ConsoleReadMutexTaken(state)) {
        emulatorLog("[MUTEX] acquireConsoleReadMutex: pid=%d blocked", getActivePCB(emu)->ProcessID);
        return false;
    }
    mutex->ConsoleRead = getActivePCB(emu)->ProcessID;
    emulatorLog("[MUTEX] acquireConsoleReadMutex: pid=%d acquired", mutex->ConsoleRead);
    return true;
}

/* Releases the console read mutex if owned by the active process. Returns true on success or false otherwise. */
bool releaseConsoleReadMutex(kernal_state *state, Emulator *emu) {
    Mutex *mutex = state->mutexes;
    if (mutex->ConsoleRead == getActivePCB(emu)->ProcessID) {
        mutex->ConsoleRead = -1;
        emulatorLog("[MUTEX] releaseConsoleReadMutex: pid=%d released", getActivePCB(emu)->ProcessID);
        return true;
    }
    emulatorLog("[MUTEX] releaseConsoleReadMutex: pid=%d failed", getActivePCB(emu)->ProcessID);
    return false;
}

/* Attempts to acquire the console write mutex for the active process. Returns true on success, false if blocked. */
bool acquireConsoleWriteMutex(kernal_state *state, Emulator *emu) {
    Mutex *mutex = state->mutexes;
    if (mutex->ConsoleWrite == getActivePCB(emu)->ProcessID) {
        emulatorLog("[MUTEX] acquireConsoleWriteMutex: pid=%d already owns", getActivePCB(emu)->ProcessID);
        return true;
    }
    if (ConsoleWriteMutexTaken(state)) {
        emulatorLog("[MUTEX] acquireConsoleWriteMutex: pid=%d blocked", getActivePCB(emu)->ProcessID);
        return false;
    }
    mutex->ConsoleWrite = getActivePCB(emu)->ProcessID;
    emulatorLog("[MUTEX] acquireConsoleWriteMutex: pid=%d acquired", mutex->ConsoleWrite);
    return true;
}

/* Releases the console write mutex if owned by the active process. Returns true on success or false otherwise. */
bool releaseConsoleWriteMutex(kernal_state *state, Emulator *emu) {
    Mutex *mutex = state->mutexes;
    if (mutex->ConsoleWrite == getActivePCB(emu)->ProcessID) {
        mutex->ConsoleWrite = -1;
        emulatorLog("[MUTEX] releaseConsoleWriteMutex: pid=%d released", getActivePCB(emu)->ProcessID);
        return true;
    }
    emulatorLog("[MUTEX] releaseConsoleWriteMutex: pid=%d failed", getActivePCB(emu)->ProcessID);
    return false;
}

/* Returns true if the active process associated with the emulator currently holds the console read mutex. */
bool hasConsoleReadMutex(kernal_state *state, Emulator *emu) {
    Mutex *mutex = state->mutexes;
    return mutex->ConsoleRead == getActivePCB(emu)->ProcessID;
}

/* Returns true if the active process associated with the emulator currently holds the console write mutex. */
bool hasConsoleWriteMutex(kernal_state *state, Emulator *emu) {
    Mutex *mutex = state->mutexes;
    return mutex->ConsoleWrite == getActivePCB(emu)->ProcessID;
}

/* Returns true if the console write mutex is currently locked by a process. */
bool ConsoleWriteMutexTaken(kernal_state *state) {
    Mutex *mutex = state->mutexes;
    return mutex->ConsoleWrite != -1;
}
