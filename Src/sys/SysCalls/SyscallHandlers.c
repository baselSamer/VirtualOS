#include "SyscallDispatcher.h"
#include "../../emu/Console/Console.h"
#include "../../emu/FIles/Files.h"
#include "../../emu/Mem/Mem.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Print syscall handler */
SyscallResultData syscall_print(
    Emulator *emu,
    kernal_state *state,
    const Instruction *instr) {
    
    SyscallResultData result = {0};
    result.code = SYSCALL_SUCCESS;
    result.output = NULL;
    result.data = NULL;
    result.blocked = 0;

    if (instr == NULL || instr->arg1 == NULL) {
        result.code = SYSCALL_INVALID_ARGS;
        return result;
    }

    /* Acquire console write mutex */
    if (state->mutexes->ConsoleWrite != 1) {
        state->mutexes->ConsoleWrite = 1;
    }

    /* Print the argument */
    int len = strlen(instr->arg1) + 1;
    result.output = malloc(len);
    if (result.output != NULL) {
        strcpy(result.output, instr->arg1);
    }

    printToConsole("%s", instr->arg1);
    state->flags->blocked = BLOCKED_NONE;

    return result;
}

/* Assign syscall handler */
SyscallResultData syscall_assign(
    Emulator *emu,
    kernal_state *state,
    const Instruction *instr) {
    
    SyscallResultData result = {0};
    result.code = SYSCALL_SUCCESS;
    result.output = NULL;
    result.data = NULL;
    result.blocked = 0;

    if (instr == NULL || instr->arg1 == NULL || instr->arg2 == NULL) {
        result.code = SYSCALL_INVALID_ARGS;
        return result;
    }

    /* In a full implementation, this would store variable in memory */
    /* For now, we log the assignment */
    state->flags->blocked = BLOCKED_NONE;

    return result;
}

/* Read file syscall handler */
SyscallResultData syscall_readFile(
    Emulator *emu,
    kernal_state *state,
    const Instruction *instr) {
    
    SyscallResultData result = {0};
    result.code = SYSCALL_SUCCESS;
    result.output = NULL;
    result.data = NULL;
    result.blocked = 0;

    if (instr == NULL || instr->arg1 == NULL) {
        result.code = SYSCALL_INVALID_ARGS;
        return result;
    }

    /* Check file mutex */
    if (!checkFileAccess(state, instr->arg1)) {
        /* Resource not available, set blocked flag */
        state->flags->blocked = BLOCKED_FILE;
        strcpy(state->flags->blocked_file, instr->arg1);
        result.blocked = 1;
        result.code = SYSCALL_BLOCKED;
        return result;
    }

    /* Read file using emulator API */
    void *file_data = NULL;
    size_t file_size = read_raw_data(instr->arg1, &file_data);

    if (file_size == 0 || file_data == NULL) {
        result.code = SYSCALL_ERROR;
        state->flags->blocked = BLOCKED_NONE;
        return result;
    }

    result.data = file_data;
    state->flags->blocked = BLOCKED_NONE;

    return result;
}

/* Write file syscall handler */
SyscallResultData syscall_writeFile(
    Emulator *emu,
    kernal_state *state,
    const Instruction *instr) {
    
    SyscallResultData result = {0};
    result.code = SYSCALL_SUCCESS;
    result.output = NULL;
    result.data = NULL;
    result.blocked = 0;

    if (instr == NULL || instr->arg1 == NULL || instr->arg2 == NULL) {
        result.code = SYSCALL_INVALID_ARGS;
        return result;
    }

    /* Check file mutex */
    if (!checkFileAccess(state, instr->arg1)) {
        /* Resource not available, set blocked flag */
        state->flags->blocked = BLOCKED_FILE;
        strcpy(state->flags->blocked_file, instr->arg1);
        result.blocked = 1;
        result.code = SYSCALL_BLOCKED;
        return result;
    }

    /* Write file using emulator API */
    int write_result = write_raw_data(instr->arg1, instr->arg2, strlen(instr->arg2));

    if (write_result != 0) {
        result.code = SYSCALL_ERROR;
        state->flags->blocked = BLOCKED_NONE;
        return result;
    }

    state->flags->blocked = BLOCKED_NONE;
    return result;
}

/* Print from-to syscall handler */
SyscallResultData syscall_printFromTo(
    Emulator *emu,
    kernal_state *state,
    const Instruction *instr) {
    
    SyscallResultData result = {0};
    result.code = SYSCALL_SUCCESS;
    result.output = NULL;
    result.data = NULL;
    result.blocked = 0;

    if (instr == NULL || instr->arg1 == NULL || instr->arg2 == NULL) {
        result.code = SYSCALL_INVALID_ARGS;
        return result;
    }

    /* Acquire console write mutex */
    if (state->mutexes->ConsoleWrite != 1) {
        state->mutexes->ConsoleWrite = 1;
    }

    /* Parse range arguments and print */
    /* In full implementation, extract values and print range */
    printToConsole("Range: %s to %s", instr->arg1, instr->arg2);
    state->flags->blocked = BLOCKED_NONE;

    return result;
}

/* Semaphore wait syscall handler */
SyscallResultData syscall_semWait(
    Emulator *emu,
    kernal_state *state,
    const Instruction *instr) {
    
    SyscallResultData result = {0};
    result.code = SYSCALL_SUCCESS;
    result.output = NULL;
    result.data = NULL;
    result.blocked = 0;

    if (instr == NULL || instr->resource == RES_INVALID) {
        result.code = SYSCALL_INVALID_ARGS;
        return result;
    }

    /* Check resource type and acquire appropriate mutex */
    switch (instr->resource) {
        case RES_USER_INPUT:
            if (state->mutexes->ConsoleRead != 1) {
                state->mutexes->ConsoleRead = 1;
                state->flags->blocked = BLOCKED_NONE;
            } else {
                state->flags->blocked = BLOCKED_CON_READ;
                result.blocked = 1;
                result.code = SYSCALL_BLOCKED;
            }
            break;

        case RES_USER_OUTPUT:
            if (state->mutexes->ConsoleWrite != 1) {
                state->mutexes->ConsoleWrite = 1;
                state->flags->blocked = BLOCKED_NONE;
            } else {
                state->flags->blocked = BLOCKED_CON_WRITE;
                result.blocked = 1;
                result.code = SYSCALL_BLOCKED;
            }
            break;

        case RES_FILE:
            /* For generic file resource, check if any file mutex is available */
            if (!checkFileAccess(state, "generic_file")) {
                if (acquireFileMutex(state, "generic_file")) {
                    state->flags->blocked = BLOCKED_NONE;
                } else {
                    state->flags->blocked = BLOCKED_FILE;
                    strcpy(state->flags->blocked_file, "generic_file");
                    result.blocked = 1;
                    result.code = SYSCALL_BLOCKED;
                }
            } else {
                state->flags->blocked = BLOCKED_FILE;
                strcpy(state->flags->blocked_file, "generic_file");
                result.blocked = 1;
                result.code = SYSCALL_BLOCKED;
            }
            break;

        default:
            result.code = SYSCALL_INVALID_ARGS;
            break;
    }

    return result;
}

/* Semaphore signal syscall handler */
SyscallResultData syscall_semSignal(
    Emulator *emu,
    kernal_state *state,
    const Instruction *instr) {
    
    SyscallResultData result = {0};
    result.code = SYSCALL_SUCCESS;
    result.output = NULL;
    result.data = NULL;
    result.blocked = 0;

    if (instr == NULL || instr->resource == RES_INVALID) {
        result.code = SYSCALL_INVALID_ARGS;
        return result;
    }

    /* Release appropriate mutex based on resource type */
    switch (instr->resource) {
        case RES_USER_INPUT:
            state->mutexes->ConsoleRead = -1;
            state->flags->unblocked_con_read = 1;
            state->flags->blocked = BLOCKED_NONE;
            break;

        case RES_USER_OUTPUT:
            state->mutexes->ConsoleWrite = -1;
            state->flags->unblocked_con_write = 1;
            state->flags->blocked = BLOCKED_NONE;
            break;

        case RES_FILE:
            if (releaseFileMutex(state, "generic_file")) {
                strcpy(state->flags->unblocked_file, "generic_file");
                state->flags->blocked = BLOCKED_NONE;
            } else {
                result.code = SYSCALL_ERROR;
            }
            break;

        default:
            result.code = SYSCALL_INVALID_ARGS;
            break;
    }

    return result;
}