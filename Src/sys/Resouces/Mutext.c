#include "Mutex.h"
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include "../../emu/Core/Logger.h"

bool fileMutexTaken(kernal_state *state, const char *path) {
    Mutex *mutex = state->mutexes;
    Node *current = mutex->file_mutexes;
    while (current != NULL) {
        if (strcmp(current->path, path) == 0) {
            if (current->id != -1) {
                emulatorLog("[MUTEX] fileMutexTaken: path='%s' taken by pid=%d", path, current->id);
                return true;
            }
            return false;
        }
        current = current->next;
    }
    emulatorLog("[MUTEX] fileMutexTaken: path='%s' free", path);
    return false;
}

bool hasFileMutex(kernal_state *state, const char *path, Emulator *emu) {
    Mutex *mutex = state->mutexes;
    Node *current = mutex->file_mutexes;
    while (current != NULL) {
        if (strcmp(current->path, path) == 0 && current->id == getActivePCB(emu)->ProcessID) {
            emulatorLog("[MUTEX] hasFileMutex: pid=%d owns path='%s'", current->id, path);
            return true;
        }
        current = current->next;
    }
    emulatorLog("[MUTEX] hasFileMutex: pid=%d does not own path='%s'", getActivePCB(emu)->ProcessID, path);
    return false;
}

bool releaseFileMutex(kernal_state *state, const char *path, Emulator *emu) {
    Mutex *mutex = state->mutexes;
    Node *current = mutex->file_mutexes;
    Node *prev = NULL;
    while (current != NULL) {
        if (strcmp(current->path, path) == 0 && current->id == getActivePCB(emu)->ProcessID) {
            current->id = -1; // Release but keep node alive
            
            // Only free if no one is waiting!
            if (current->blocked_queue.count == 0) {
                if (prev == NULL) {
                    mutex->file_mutexes = current->next;
                } else {
                    prev->next = current->next;
                }
                free(current->path);
                free(current);
                emulatorLog("[MUTEX] releaseFileMutex: pid=%d released path='%s' and node destroyed", getActivePCB(emu)->ProcessID, path);
            } else {
                emulatorLog("[MUTEX] releaseFileMutex: pid=%d released path='%s' (processes still waiting)", getActivePCB(emu)->ProcessID, path);
            }
            return true;
        }
        prev = current;
        current = current->next;
    }
    emulatorLog("[MUTEX] releaseFileMutex: pid=%d failed to release path='%s'", getActivePCB(emu)->ProcessID, path);
    return false; // Mutex not found for this process
}

bool acquireFileMutex(kernal_state *state, const char *path, Emulator *emu) {
    if (hasFileMutex(state, path, emu)) {
        emulatorLog("[MUTEX] acquireFileMutex: pid=%d already owns path='%s'", getActivePCB(emu)->ProcessID, path);
        return true; // Already has mutex
    }
    if (fileMutexTaken(state, path)) {
        emulatorLog("[MUTEX] acquireFileMutex: pid=%d blocked for path='%s'", getActivePCB(emu)->ProcessID, path);
        return false; // Mutex is taken by another process
    }
    Mutex *mutex = state->mutexes;
    
    // Check if the node already exists but is unowned
    Node *current = mutex->file_mutexes;
    while (current != NULL) {
        if (strcmp(current->path, path) == 0 && current->id == -1) {
            current->id = getActivePCB(emu)->ProcessID;
            emulatorLog("[MUTEX] acquireFileMutex: pid=%d claimed existing vacant path='%s'", current->id, path);
            return true;
        }
        current = current->next;
    }
    
    Node *new_node = (Node*)malloc(sizeof(Node));
    new_node->path = strdup(path);
    new_node->id = getActivePCB(emu)->ProcessID;
    initQueue(&new_node->blocked_queue);
    
    new_node->next = mutex->file_mutexes;
    mutex->file_mutexes = new_node;
    emulatorLog("[MUTEX] acquireFileMutex: pid=%d acquired NEW path='%s'", new_node->id, path);
    return true;
}

// Additional mutex functions for console I/O can be implemented similarly, using the ConsoleRead and ConsoleWrite fields in the Mutext struct to track which process has the mutex for console operations.
bool ConsoleReadMutexTaken(kernal_state *state) {
    Mutex *mutex = state->mutexes;
    return mutex->ConsoleRead != -1;
}

bool ConsoleWriteMutexTaken(kernal_state *state) {
    Mutex *mutex = state->mutexes;
    return mutex->ConsoleWrite != -1;
}

bool acquireConsoleReadMutex(kernal_state *state, Emulator *emu) {
    Mutex *mutex = state->mutexes;
    if (mutex->ConsoleRead == getActivePCB(emu)->ProcessID) {
        emulatorLog("[MUTEX] acquireConsoleReadMutex: pid=%d already owns", getActivePCB(emu)->ProcessID);
        return true; // Already has mutex
    }
    if (ConsoleReadMutexTaken(state)) {
        emulatorLog("[MUTEX] acquireConsoleReadMutex: pid=%d blocked", getActivePCB(emu)->ProcessID);
        return false; // Mutex is taken by another process
    }
    mutex->ConsoleRead = getActivePCB(emu)->ProcessID;
    emulatorLog("[MUTEX] acquireConsoleReadMutex: pid=%d acquired", mutex->ConsoleRead);
    return true;
}

bool releaseConsoleReadMutex(kernal_state *state, Emulator *emu) {
    Mutex *mutex = state->mutexes;
    if (mutex->ConsoleRead == getActivePCB(emu)->ProcessID) {
        mutex->ConsoleRead = -1;
        emulatorLog("[MUTEX] releaseConsoleReadMutex: pid=%d released", getActivePCB(emu)->ProcessID);
        return true;
    }
    emulatorLog("[MUTEX] releaseConsoleReadMutex: pid=%d failed", getActivePCB(emu)->ProcessID);
    return false; // Mutex not held by this process
}

bool acquireConsoleWriteMutex(kernal_state *state, Emulator *emu) {
    Mutex *mutex = state->mutexes;
    if (mutex->ConsoleWrite == getActivePCB(emu)->ProcessID) {
        emulatorLog("[MUTEX] acquireConsoleWriteMutex: pid=%d already owns", getActivePCB(emu)->ProcessID);
        return true; // Already has mutex
    }
    if (ConsoleWriteMutexTaken(state)) {
        emulatorLog("[MUTEX] acquireConsoleWriteMutex: pid=%d blocked", getActivePCB(emu)->ProcessID);
        return false; // Mutex is taken by another process
    }
    mutex->ConsoleWrite = getActivePCB(emu)->ProcessID;
    emulatorLog("[MUTEX] acquireConsoleWriteMutex: pid=%d acquired", mutex->ConsoleWrite);
    return true;
}

bool releaseConsoleWriteMutex(kernal_state *state, Emulator *emu) {
    Mutex *mutex = state->mutexes;
    if (mutex->ConsoleWrite == getActivePCB(emu)->ProcessID) {
        mutex->ConsoleWrite = -1;
        emulatorLog("[MUTEX] releaseConsoleWriteMutex: pid=%d released", getActivePCB(emu)->ProcessID);
        return true;
    }
    emulatorLog("[MUTEX] releaseConsoleWriteMutex: pid=%d failed", getActivePCB(emu)->ProcessID);
    return false; // Mutex not held by this process
}

bool hasConsoleReadMutex(kernal_state *state, Emulator *emu) {
    Mutex *mutex = state->mutexes;
    return mutex->ConsoleRead == getActivePCB(emu)->ProcessID;
}

bool hasConsoleWriteMutex(kernal_state *state, Emulator *emu) {
    Mutex *mutex = state->mutexes;
    return mutex->ConsoleWrite == getActivePCB(emu)->ProcessID;
}




