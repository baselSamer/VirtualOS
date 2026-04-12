#include "kernal.h"

void execute(Emulator *emu, kernal_state *state) {
    if (state -> flags) 
        free(state->flags);
    PCB *pcb = getActivePCB(emu);
    if (pcb->ProcessID==0) {
        // Run UI(emu);
    } else {
       // Run interpeter(emu);
        pcb->pc += 1; 
        state->current_tick_count += 1;
        if (pcb->pc >= pcb->bounds[2]) {
            pcb->state = TERMINATED;
        }
    }
}

int start_exution(Emulator *emu, kernal_state *state) {
        printToConsole("Entering execution loop...");
        while (1) {
            // scheduler(emu, state);
            execute(emu, state);
        }
        return 0;
}