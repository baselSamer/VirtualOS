#include "../emu/Console/Console.h"
#include "../emu/Core/Logger.h"
#include "../emu/Mem/Mem.h"
#include "../emu/Core/Emulator_core.h"
#include "../emu/Mem/Mem_core.h"
#include "kernal.h"
#include "Gui/Gui.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

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
    for (int i = 0; i < 40; i++) {
        freeMemLoc(emu, i);
    }
    setActivePCB(emu, NULL);
}

static kernal_state* createKernalState(void) {
    kernal_state *state = malloc(sizeof(kernal_state));

    state->current_tick_count = 0;
    state->gui_mode = 0;
    state->auto_release_resources = 0;
    state->skip_empty_lines_on_load = 0;

    state->flags = NULL;
    createNewFlags(state);

    state->mutexes = malloc(sizeof(Mutex));
    state->mutexes->ConsoleRead = -1;
    state->mutexes->ConsoleWrite = -1;
    state->mutexes->File = -1;
    initQueue(&state->mutexes->input_queue);
    initQueue(&state->mutexes->output_queue);
    initQueue(&state->mutexes->file_queue);

    initQueue(&state->ready_queue);
    initQueue(&state->ready_queue_1);
    initQueue(&state->ready_queue_2);
    initQueue(&state->ready_queue_3);
    initQueue(&state->general_blocked_queue);

    state->current_algo = 0;
    state->time_quantum = 0;
    state->rr_time_quantum_counter = 0;
    state->mlfq_time_quantum_counter = 0;
    state->active_process_queue_index = 0;
    state->num_scheduled_processes = 0;
    state->scheduled_processes = NULL;
    state->current_running_pid = -1;
    state->current_running_command[0] = '\0';

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
    printBootLogo();

    /* Mode selection */
    printToConsole("  Select mode:");
    printToConsole("    [1] Terminal");
    printToConsole("    [2] GUI + Terminal");
    printToConsole("");
    printToConsole("  Choice: ");

    int mode_choice = 1;
    readFromConsole("%d", &mode_choice);

    while (1) {
        kernal_state *state = createKernalState();
        state->gui_mode = (mode_choice == 2) ? 1 : 0;
        emulatorLog("[KERNEL_STARTUP] kernel state initialized (tick=%d, gui=%d)", state->current_tick_count, state->gui_mode);

        /* Start GUI if selected */
        if (state->gui_mode) {
            startGui(emu, state);
            printToConsole("");
            printToConsole("  GUI window opened. Use the Step button to advance ticks.");
            printToConsole("  Terminal remains active for process logs.");
            printToConsole("");
        }

        start_exution(emu, state);

        /* Stop GUI before cleanup */
        if (state->gui_mode) {
            stopGui();
        }

        freeKernalState(state);

        /* Ask to restart */
        printToConsole("");
        printToConsole("+--------------------------------------+");
        printToConsole("|         SIMULATION ENDED             |");
        printToConsole("+--------------------------------------+");
        printToConsole("");
        printToConsole("  Restart simulation? (y/n): ");

        int c;
        while ((c = getchar()) != '\n' && c != EOF);
        c = getchar();

        if (c != 'y' && c != 'Y') {
            printToConsole("");
            printToConsole("  Shutting down Project Ethos...");
            printToConsole("");
            break;
        }

        printToConsole("");
        printToConsole("  Resetting system...");
        printToConsole("");
        resetEmulatorMemory(emu);
    }

    emulatorLog("[KERNEL_STARTUP] start() returned");
    return 0;
}