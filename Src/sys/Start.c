#include "../emu/Console/Console.h"
#include "../emu/Core/Logger.h"
#include "../emu/Mem/Mem.h"
#include "../emu/Core/Emulator_core.h"
#include "../emu/Mem/Mem_core.h"
#include "kernal.h"
#include <stdlib.h>
#include <stdio.h>

static void printBootLogo(void) {
    printToConsole("");
    printToConsole("    +====================================================+");
    printToConsole("    |                                                      |");
    printToConsole("    |    ######  ########  ##    ##  #######   ######      |");
    printToConsole("    |    ##         ##     ##    ## ##     ## ##           |");
    printToConsole("    |    #####      ##     ######## ##     ##  #####      |");
    printToConsole("    |    ##         ##     ##    ## ##     ##      ##     |");
    printToConsole("    |    ######     ##     ##    ##  #######  ######      |");
    printToConsole("    |                                                      |");
    printToConsole("    |              Project Ethos OS Simulator              |");
    printToConsole("    |                      v1.0.0                          |");
    printToConsole("    |                                                      |");
    printToConsole("    +====================================================+");
    printToConsole("");
}

static void resetEmulatorMemory(Emulator *emu) {
    // Free all memory words in the 40-slot RAM
    for (int i = 0; i < 40; i++) {
        freeMemLoc(emu, i);
    }
    // Clear the active PCB
    setActivePCB(emu, NULL);
}

static kernal_state* createKernalState(void) {
    kernal_state *state = malloc(sizeof(kernal_state));

    state->current_tick_count = 0;

    // Initialize flags
    state->flags = NULL;
    createNewFlags(state);

    // Initialize mutexes
    state->mutexes = malloc(sizeof(Mutex));
    state->mutexes->ConsoleRead = -1;
    state->mutexes->ConsoleWrite = -1;
    state->mutexes->file_mutexes = NULL;
    initQueue(&state->mutexes->input_queue);
    initQueue(&state->mutexes->output_queue);

    // Initialize all ready queues
    initQueue(&state->ready_queue);
    initQueue(&state->ready_queue_1);
    initQueue(&state->ready_queue_2);
    initQueue(&state->ready_queue_3);
    initQueue(&state->general_blocked_queue);

    // Initialize scheduler state (will be configured by console)
    state->current_algo = 0;
    state->time_quantum = 0;
    state->rr_time_quantum_counter = 0;
    state->mlfq_time_quantum_counter = 0;
    state->active_process_queue_index = 0;
    state->num_scheduled_processes = 0;
    state->scheduled_processes = NULL;

    return state;
}

static void freeKernalState(kernal_state *state) {
    if (state->scheduled_processes != NULL) {
        for (int i = 0; i < state->num_scheduled_processes; i++) {
            if (state->scheduled_processes[i].filepath != NULL) {
                free(state->scheduled_processes[i].filepath);
            }
        }
        free(state->scheduled_processes);
    }
    if (state->flags != NULL) free(state->flags);
    if (state->mutexes != NULL) free(state->mutexes);
    free(state);
}

int start(Emulator *emu) {
    emulatorLog("[KERNEL_STARTUP] start() called");

    // Show boot logo
    printBootLogo();

    while (1) {
        // Create a fresh kernel state
        kernal_state *state = createKernalState();
        emulatorLog("[KERNEL_STARTUP] kernel state initialized (tick=%d)", state->current_tick_count);

        // Run the execution loop
        start_exution(emu, state);

        // Cleanup current state
        freeKernalState(state);

        // Ask user if they want to restart
        printToConsole("");
        printToConsole("+--------------------------------------+");
        printToConsole("|         SIMULATION ENDED             |");
        printToConsole("+--------------------------------------+");
        printToConsole("");
        printToConsole("  Restart simulation? (y/n): ");

        int c;
        // Flush leftover input
        while ((c = getchar()) != '\n' && c != EOF);
        c = getchar();

        if (c != 'y' && c != 'Y') {
            printToConsole("");
            printToConsole("  Shutting down Project Ethos...");
            printToConsole("");
            break;
        }

        // Reset emulator memory for a fresh start
        printToConsole("");
        printToConsole("  Resetting system...");
        printToConsole("");
        resetEmulatorMemory(emu);
    }

    emulatorLog("[KERNEL_STARTUP] start() returned");
    return 0;
}