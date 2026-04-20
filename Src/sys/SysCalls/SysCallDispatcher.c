#include "SyscallDispatcher.h"
#include "../../emu/Core/Logger.h"
#include <stdlib.h>
#include <string.h>

/* Dispatcher routes instruction to appropriate handler */
SyscallResultData dispatchSyscall(
    Emulator *emu,
    kernal_state *state,
    const Instruction *instr,
    int pid) {
    
    SyscallResultData result = {0};
    result.code = SYSCALL_NOT_FOUND;
    result.output = NULL;
    result.data = NULL;
    result.blocked = 0;

    if (instr == NULL || !instr->valid) {
        result.code = SYSCALL_INVALID_ARGS;
        emulatorLog("[SYSCALL] pid=%d call=INVALID error=invalid_instruction", pid);
        return result;
    }

    /* Create new flags for this syscall */
    createNewFlags(state);

    /* Route to appropriate handler based on opcode */
    switch (instr->opcode) {
        case OP_PRINT:
            result = syscall_print(emu, state, instr);
            emulatorLog("[SYSCALL] pid=%d call=PRINT blocked=%d", pid, result.blocked);
            break;

        case OP_ASSIGN:
            result = syscall_assign(emu, state, instr);
            emulatorLog("[SYSCALL] pid=%d call=ASSIGN blocked=%d var=%s", pid, result.blocked, instr->arg1);
            break;

        case OP_READ_FILE:
            result = syscall_readFile(emu, state, instr);
            emulatorLog("[SYSCALL] pid=%d call=READ_FILE blocked=%d path=%s", pid, result.blocked, instr->arg1);
            break;

        case OP_WRITE_FILE:
            result = syscall_writeFile(emu, state, instr);
            emulatorLog("[SYSCALL] pid=%d call=WRITE_FILE blocked=%d path=%s", pid, result.blocked, instr->arg1);
            break;

        case OP_PRINT_FROM_TO:
            result = syscall_printFromTo(emu, state, instr);
            emulatorLog("[SYSCALL] pid=%d call=PRINT_FROM_TO blocked=%d", pid, result.blocked);
            break;

        case OP_SEM_WAIT:
            result = syscall_semWait(emu, state, instr);
            emulatorLog("[SYSCALL] pid=%d call=SEM_WAIT blocked=%d resource=%s", pid, result.blocked, instr->arg1);
            break;

        case OP_SEM_SIGNAL:
            result = syscall_semSignal(emu, state, instr);
            emulatorLog("[SYSCALL] pid=%d call=SEM_SIGNAL blocked=%d resource=%s", pid, result.blocked, instr->arg1);
            break;

        default:
            result.code = SYSCALL_NOT_FOUND;
            emulatorLog("[SYSCALL] pid=%d call=UNKNOWN error=opcode_not_found", pid);
            break;
    }

    return result;
}

/* Free syscall result data */
void freeSyscallResult(SyscallResultData *result) {
    if (result == NULL) {
        return;
    }
    if (result->output != NULL) {
        free(result->output);
        result->output = NULL;
    }
    if (result->data != NULL) {
        free(result->data);
        result->data = NULL;
    }
}

/* Check if process has access to file */
int checkFileAccess(kernal_state *state, const char *file_path) {
    if (state == NULL || file_path == NULL) {
        return 0;
    }
    
    if (state->mutexes == NULL) {
        return 0;
    }

    Node *current = state->mutexes->file_mutexes;
    while (current != NULL) {
        if (strcmp(current->path, file_path) == 0) {
            return 1;
        }
        current = current->next;
    }
    return 0;
}