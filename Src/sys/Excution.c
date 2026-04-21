#include "kernal.h"
#include "Memory/Memory.h"
#include "../emu/Mem/Mem.h"
#include "../emu/Console/Console.h"
#include "../emu/Core/Logger.h"
#include "Gui/Gui.h"
#include "../apps/systemApps/Parser_Interpeter/Parser.h"
#include <stdlib.h>
#include <stdio.h>

static void waitForEnter(void) {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
    getchar();
}

/* Execute single instruction from current pc */
void execute(Emulator *emu, kernal_state *state) {
    scheduler(emu, state);

    PCB *pcb = getActivePCB(emu);

    if (pcb != NULL && pcb->state == RUNNING) {
        struct MemoryWord *word = (struct MemoryWord*)readMem(emu, pcb->pc);

        if (word != NULL && word->type == MEM_TYPE_INSTRUCTION) {
            int is_blocked = parse_and_execute(word->content.inst_data.code_line, pcb->ProcessID, state, emu);
            if (!is_blocked) {
                pcb->pc += 1;
            }
        } else {
            printToConsole("  | PCB: %-3d | [no instruction at PC %d]", pcb->ProcessID, pcb->pc);
            pcb->pc += 1;
        }

        if (pcb->pc > pcb->bounds[3]) {
            pcb->state = TERMINATED;
            printToConsole("  | PCB: %-3d | *** TERMINATED ***", pcb->ProcessID);
        }
    } else {
        printToConsole("  | CPU IDLE | No process running");
    }

    state->current_tick_count += 1;
}

int start_exution(Emulator *emu, kernal_state *state) {
    emulatorLog("[KERNEL] Execution loop started");

    if (state->gui_mode) {
        waitForGuiConfig();
    } else {
        initSchedulerConfig(emu, state);
    }

    /* Update GUI after config so it shows initial state */
    if (state->gui_mode) {
        updateGui();
    }

    printToConsole("+--------------------------------------+");
    printToConsole("|         EXECUTION STARTED            |");
    printToConsole("+--------------------------------------+");
    printToConsole("");

    while (1) {
        printToConsole("+======================================+");
        printToConsole("|  TICK %-5d                          |", state->current_tick_count);
        printToConsole("+--------------------------------------+");

        execute(emu, state);

        printToConsole("+======================================+");

        /* Check completion */
        PCB *pcb = getActivePCB(emu);
        if (pcb == NULL && isEmpty(&state->ready_queue) 
            && isEmpty(&state->ready_queue_1) && isEmpty(&state->ready_queue_2) 
            && isEmpty(&state->ready_queue_3) && isEmpty(&state->general_blocked_queue)
            && state->current_tick_count > 0) {

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

                /* Final GUI update */
                if (state->gui_mode) {
                    updateGui();
                }
                break;
            }
        }

        /* Wait for next tick */
        if (state->gui_mode) {
            /* In GUI mode: wait for Step button click */
            waitForGuiStep();
        } else {
            /* In terminal mode: wait for Enter key */
            printToConsole("");
            printToConsole("  Press [Enter] to continue...");
            waitForEnter();
        }
    }

    emulatorLog("[KERNEL] Execution loop finished at tick %d", state->current_tick_count);
    return 0;
}