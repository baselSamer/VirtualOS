#include "kernal.h"
#include "Memory/Memory.h"
#include "../emu/Mem/Mem.h"
#include "../emu/Console/Console.h"
#include "../emu/Core/Logger.h"
#include <stdlib.h>
#include <stdio.h>

static void waitForEnter(void) {
    int c;
    // Flush any leftover characters in stdin
    while ((c = getchar()) != '\n' && c != EOF);
    // Wait for the actual Enter press
    getchar();
}

/* Execute single instruction from current pc */
void execute(Emulator *emu, kernal_state *state) {
    // Step 1: Run the scheduler to pick/manage the active process
    scheduler(emu, state);

    // Step 2: Simulate instruction execution
    PCB *pcb = getActivePCB(emu);

    if (pcb != NULL && pcb->state == RUNNING) {
        // Read the memory word at the current PC
        struct MemoryWord *word = (struct MemoryWord*)readMem(emu, pcb->pc);

        if (word != NULL && word->type == MEM_TYPE_INSTRUCTION) {
            printToConsole("  | PCB: %-3d | Command: %s", pcb->ProcessID, word->content.inst_data.code_line);
        } else {
            printToConsole("  | PCB: %-3d | [no instruction at PC %d]", pcb->ProcessID, pcb->pc);
        }

        // Advance PC
        pcb->pc += 1;

        // Check if process has finished all its instructions
        if (pcb->pc > pcb->bounds[3]) {
            pcb->state = TERMINATED;
            printToConsole("  | PCB: %-3d | *** TERMINATED ***", pcb->ProcessID);
        }
    } else {
        printToConsole("  | CPU IDLE | No process running");
    }

    // Step 3: Clear flags
    createNewFlags(state);

    // Step 4: Increase tick by one
    state->current_tick_count += 1;
}

int start_exution(Emulator *emu, kernal_state *state) {
    emulatorLog("[KERNEL] Execution loop started");

    // Run the console configuration before starting
    initSchedulerConfig(emu, state);

    printToConsole("+--------------------------------------+");
    printToConsole("|         EXECUTION STARTED            |");
    printToConsole("+--------------------------------------+");
    printToConsole("");

    while (1) {
        // Print tick header
        printToConsole("+======================================+");
        printToConsole("|  TICK %-5d                          |", state->current_tick_count);
        printToConsole("+--------------------------------------+");

        execute(emu, state);

        printToConsole("+======================================+");

        // Check if all processes have finished
        PCB *pcb = getActivePCB(emu);
        if (pcb == NULL && isEmpty(&state->ready_queue) 
            && isEmpty(&state->ready_queue_1) && isEmpty(&state->ready_queue_2) 
            && isEmpty(&state->ready_queue_3) && isEmpty(&state->general_blocked_queue)
            && state->current_tick_count > 0) {

            // Check if any processes haven't arrived yet
            int pending = 0;
            for (int i = 0; i < state->num_scheduled_processes; i++) {
                if (state->scheduled_processes[i].spawn_time >= state->current_tick_count) {
                    pending = 1;
                    break;
                }
            }
            if (!pending) {
                printToConsole("");
                printToConsole("+--------------------------------------+");
                printToConsole("|     ALL PROCESSES COMPLETED          |");
                printToConsole("|     Total Ticks: %-5d               |", state->current_tick_count);
                printToConsole("+--------------------------------------+");
                break;
            }
        }

        // Wait for user to press Enter before next cycle
        printToConsole("");
        printToConsole("  Press [Enter] to continue...");
        waitForEnter();
    }

    emulatorLog("[KERNEL] Execution loop finished at tick %d", state->current_tick_count);
    return 0;
}