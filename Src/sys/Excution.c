#include "kernal.h"
#include "Parser/Parser.h"
#include "SysCalls/SyscallDispatcher.h"
#include "../emu/Console/Console.h"
#include "../emu/Core/Logger.h"
#include <stdlib.h>
#include <string.h>

/* Process instruction list stored in process memory */
typedef struct {
    Instruction *instructions;
    int instruction_count;
} ProcessScript;

/* Execute single instruction from current pc */
void execute(Emulator *emu, kernal_state *state) {
    if (state->flags) 
        free(state->flags);
    
    PCB *pcb = getActivePCB(emu);
    
    if (pcb == NULL) {
        return;
    }

    /* UI process (ProcessID == 0) */
    if (pcb->ProcessID == 0) {
        /* Run UI */
        emulatorLog("[EXEC] pid=0 pc=%d opcode=UI", pcb->pc);
    } else {
        /* User process - fetch and execute instruction */
        
        /* Check pc bounds */
        if (pcb->pc < pcb->bounds[0] || pcb->pc >= pcb->bounds[2]) {
            emulatorLog("[EXEC] pid=%d pc=%d error=out_of_bounds", pcb->ProcessID, pcb->pc);
            pcb->state = TERMINATED;
            state->current_tick_count += 1;
            return;
        }

        /* Fetch instruction from process memory */
        /* In full implementation, this would read from process memory */
        /* For now, we'll parse from instruction array if available */
        
        emulatorLog("[EXEC] pid=%d pc=%d opcode=FETCH", pcb->ProcessID, pcb->pc);

        /* Simulate instruction fetch and execution */
        /* This would be replaced with actual memory read */
        
        /* Create a dummy instruction for now */
        Instruction instr = {0};
        instr.opcode = OP_PRINT;
        instr.arg1 = malloc(6);
        strcpy(instr.arg1, "hello");
        instr.valid = 1;

        /* Dispatch syscall */
        SyscallResultData syscall_result = dispatchSyscall(
            emu, 
            state, 
            &instr, 
            pcb->ProcessID
        );

        emulatorLog("[EXEC] pid=%d pc=%d opcode=PRINT result=%d", 
                    pcb->ProcessID, pcb->pc, syscall_result.code);

        /* Advance pc only on success */
        if (syscall_result.code == SYSCALL_SUCCESS) {
            pcb->pc += 1;
        } else if (syscall_result.code == SYSCALL_BLOCKED) {
            /* Do not advance pc if blocked */
            emulatorLog("[EXEC] pid=%d pc=%d status=blocked", pcb->ProcessID, pcb->pc);
        }

        /* Check bounds again after increment */
        if (pcb->pc >= pcb->bounds[2]) {
            pcb->state = TERMINATED;
            emulatorLog("[EXEC] pid=%d status=terminated", pcb->ProcessID);
        }

        /* Clean up */
        freeSyscallResult(&syscall_result);
        freeInstruction(&instr);
    }

    state->current_tick_count += 1;
}

int start_exution(Emulator *emu, kernal_state *state) {
    printToConsole("Entering execution loop...");
    emulatorLog("[KERNEL] Execution loop started");
    
    while (1) {
        /* In full implementation, scheduler would select next PCB */
        /* scheduler(emu, state); */
        
        execute(emu, state);
        
        /* Add exit condition check */
        PCB *pcb = getActivePCB(emu);
        if (pcb != NULL && pcb->state == TERMINATED) {
            emulatorLog("[KERNEL] Process %d terminated at tick %d", 
                        pcb->ProcessID, state->current_tick_count);
        }
    }
    return 0;
}