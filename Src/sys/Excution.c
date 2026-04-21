#include "kernal.h"
#include "Memory/Memory.h"
#include "../emu/Mem/Mem.h"
#include "../emu/Console/Console.h"
#include "../emu/Core/Logger.h"
#include "Gui/Gui.h"
#include "../apps/systemApps/Parser_Interpeter/Parser.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static const char* pcbStateToStr(enum State state) {
    switch (state) {
        case CREATED: return "CREATED";
        case READY: return "READY";
        case RUNNING: return "RUNNING";
        case BLOCKED: return "BLOCKED";
        case TERMINATED: return "TERMINATED";
        default: return "UNKNOWN";
    }
}

static int findOwnerPidForSlot(Emulator *emu, int slot) {
    for (int i = 0; i < 40; i++) {
        struct MemoryWord *word = (struct MemoryWord*)readMem(emu, i);
        if (word != NULL && word->type == MEM_TYPE_PCB) {
            int start = word->content.pcb_data.bounds[0];
            int end = word->content.pcb_data.bounds[1];
            if (slot >= start && slot <= end) {
                return word->content.pcb_data.ProcessID;
            }
        }
    }
    return -1;
}

static void printMemorySnapshot(Emulator *emu) {
    printToConsole("  | MEMORY  | Snapshot (40 slots)");
    for (int i = 0; i < 40; i++) {
        struct MemoryWord *word = (struct MemoryWord*)readMem(emu, i);
        if (word == NULL) {
            printToConsole("  | MEM[%02d] | FREE", i);
            continue;
        }

        if (word->type == MEM_TYPE_PCB) {
            PCB *pcb = &word->content.pcb_data;
            printToConsole("  | MEM[%02d] | PCB PID=%d State=%s PC=%d Bounds=[%d,%d,%d,%d]",
                i,
                pcb->ProcessID,
                pcbStateToStr(pcb->state),
                pcb->pc,
                pcb->bounds[0], pcb->bounds[1], pcb->bounds[2], pcb->bounds[3]);
        } else if (word->type == MEM_TYPE_VARIABLE) {
            int owner = findOwnerPidForSlot(emu, i);
            printToConsole("  | MEM[%02d] | VAR Owner=P%d Name='%s' Value='%s'",
                i,
                owner,
                word->content.var_data.name,
                word->content.var_data.value);
        } else if (word->type == MEM_TYPE_INSTRUCTION) {
            int owner = findOwnerPidForSlot(emu, i);
            printToConsole("  | MEM[%02d] | INS Owner=P%d Code='%s'",
                i,
                owner,
                word->content.inst_data.code_line);
        } else {
            printToConsole("  | MEM[%02d] | UNKNOWN", i);
        }
    }
}

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
            state->current_running_pid = pcb->ProcessID;
            strncpy(state->current_running_command, word->content.inst_data.code_line, sizeof(state->current_running_command) - 1);
            state->current_running_command[sizeof(state->current_running_command) - 1] = '\0';
            int is_blocked = parse_and_execute(word->content.inst_data.code_line, pcb->ProcessID, state, emu);
            if (!is_blocked) {
                pcb->pc += 1;
            }
        } else {
            state->current_running_pid = pcb->ProcessID;
            strncpy(state->current_running_command, "[no instruction]", sizeof(state->current_running_command) - 1);
            state->current_running_command[sizeof(state->current_running_command) - 1] = '\0';
            printToConsole("  | PCB: %-3d | [no instruction at PC %d]", pcb->ProcessID, pcb->pc);
            pcb->pc += 1;
        }

        if (pcb->pc > pcb->bounds[3]) {
            pcb->state = TERMINATED;
            printToConsole("  | PCB: %-3d | *** TERMINATED ***", pcb->ProcessID);
        }
    } else {
        state->current_running_pid = -1;
        strncpy(state->current_running_command, "IDLE", sizeof(state->current_running_command) - 1);
        state->current_running_command[sizeof(state->current_running_command) - 1] = '\0';
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

        if (!state->gui_mode) {
            printMemorySnapshot(emu);
        }

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