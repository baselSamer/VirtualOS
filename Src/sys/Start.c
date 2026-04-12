#include "../emu/Mem/Mem.h"
#include "../emu/FIles/Files.h"
#include "../emu/Core/Logger.h"
#include "kernal.h"
#include <stdlib.h>

int start(Emulator *emu) {
    emulatorLog("[KERNEL_STARTUP] start() called");
    // Startup UI
    printToConsole("Starting Project Ethos...");
    PCB *PCB = malloc(sizeof(PCB));
    PCB->ProcessID = 0; // Unique ID for UI
    PCB->state = READY;
    PCB->pc = 0; // Not used for UI
    PCB->bounds[0] = 0; // Not used for UI
    PCB->bounds[1] = 0; // Not used for UI
    PCB->bounds[2] = 0; // Not used for UI
    PCB->bounds[3] = 0; // Not used for UI
    kernal_state *state = malloc(sizeof(kernal_state));
    state->mutexes = malloc(sizeof(Mutex));
    state->mutexes->ConsoleRead = -1;
    state->mutexes->ConsoleWrite = -1;
    state->mutexes->file_mutexes = NULL;
    state->current_tick_count = 0;
    emulatorLog("[KERNEL_STARTUP] kernel state initialized (tick=%d)", state->current_tick_count);
    start_exution(emu, state);
    emulatorLog("[KERNEL_STARTUP] start() returned");
    return 0;
}