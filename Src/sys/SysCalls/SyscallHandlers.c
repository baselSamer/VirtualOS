#include "SyscallDispatcher.h"
#include "../../emu/Console/Console.h"
#include "../../emu/FIles/Files.h"
#include "../../emu/Mem/Mem.h"
#include "../Memory/Memory.h"
#include "../Gui/Gui.h"
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
    
    int pid = getActivePCB(emu)->ProcessID;
    int implicit_lock = 0;

    /* Ensure we have console write access */
    if (state->mutexes->ConsoleWrite != -1 && state->mutexes->ConsoleWrite != pid) {
        state->flags->blocked = BLOCKED_CON_WRITE;
        result.blocked = 1;
        result.code = SYSCALL_BLOCKED;
        return result;
    }
    
    /* Implicitly acquire if free */
    if (state->mutexes->ConsoleWrite == -1) {
        state->mutexes->ConsoleWrite = pid;
        implicit_lock = 1;
    }

    /* Print the argument */
    int len = strlen(instr->arg1) + 1;
    result.output = malloc(len);
    if (result.output != NULL) {
        strcpy(result.output, instr->arg1);
    }

    char *val = get_variable(emu, pid, instr->arg1);
    if (val != NULL && strlen(val) > 0) {
        printToConsole("%s", val);
    } else {
        printToConsole("%s", instr->arg1);
    }
    
    if (implicit_lock) {
        state->mutexes->ConsoleWrite = -1;
        state->flags->unblocked_con_write = 1;
    }
    
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
    
    int pid = getActivePCB(emu)->ProcessID;
    int implicit_lock = 0;

    /* If assigning from user input, need ConsoleRead */
    if (strcmp(instr->arg2, "input") == 0) {
        if (state->mutexes->ConsoleRead != -1 && state->mutexes->ConsoleRead != pid) {
            state->flags->blocked = BLOCKED_CON_READ;
            result.blocked = 1;
            result.code = SYSCALL_BLOCKED;
            return result;
        }
        if (state->mutexes->ConsoleRead == -1) {
            state->mutexes->ConsoleRead = pid;
            implicit_lock = 1;
        }
    }

    /* Assign logic */
    if (strcmp(instr->arg2, "input") == 0) {
        if (state->gui_mode) {
            char *ret = guiWaitForInput("Please enter a value");
            if (ret != NULL) {
                set_variable(emu, pid, instr->arg1, ret);
                free(ret);
            }
        } else {
            printToConsole("Please enter a value");
            char buffer[256];
            if (scanf("%255s", buffer) > 0) {
                set_variable(emu, pid, instr->arg1, buffer);
            }
        }
    } else if (strcmp(instr->arg2, "readFile") == 0 && instr->arg3 != NULL) {
        const char *filename = instr->arg3;
        char *vname = get_variable(emu, pid, filename);
        if (vname && strlen(vname) > 0) filename = vname;

        if (!checkFileAccess(state, filename)) {
            state->flags->blocked = BLOCKED_FILE;
            strcpy(state->flags->blocked_file, filename);
            result.blocked = 1;
            result.code = SYSCALL_BLOCKED;
            if (implicit_lock) {
                state->mutexes->ConsoleRead = -1;
                state->flags->unblocked_con_read = 1;
            }
            return result;
        }
        void *file_data = NULL;
        size_t size = read_raw_data(filename, &file_data);
        if (file_data != NULL) {
            char *p = (char*)file_data;
            while (*p && (*p == '\n' || *p == '\r')) p++;
            char *end = p + strlen(p) - 1;
            while (end > p && (*end == '\n' || *end == '\r')) end--;
            end[1] = '\0';
            set_variable(emu, pid, instr->arg1, p);
            free(file_data);
        }
    } else {
        /* Standard assignment */
        char *val = get_variable(emu, pid, instr->arg2);
        set_variable(emu, pid, instr->arg1, val && strlen(val) > 0 ? val : instr->arg2);
    }
    
    if (implicit_lock) {
        state->mutexes->ConsoleRead = -1;
        state->flags->unblocked_con_read = 1;
    }
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

    int pid = getActivePCB(emu)->ProcessID;

    char *v1 = get_variable(emu, pid, instr->arg1);
    const char *resolved_arg1 = (v1 && strlen(v1) > 0) ? v1 : instr->arg1;

    /* Check file mutex */
    if (!checkFileAccess(state, resolved_arg1)) {
        /* Resource not available, set blocked flag */
        state->flags->blocked = BLOCKED_FILE;
        strcpy(state->flags->blocked_file, resolved_arg1);
        result.blocked = 1;
        result.code = SYSCALL_BLOCKED;
        return result;
    }

    /* Read file using emulator API */
    void *file_data = NULL;
    size_t file_size = read_raw_data(resolved_arg1, &file_data);

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

    int pid = getActivePCB(emu)->ProcessID;

    char *v1 = get_variable(emu, pid, instr->arg1);
    const char *resolved_arg1 = (v1 && strlen(v1) > 0) ? v1 : instr->arg1;
    char *v2 = get_variable(emu, pid, instr->arg2);
    const char *resolved_arg2 = (v2 && strlen(v2) > 0) ? v2 : instr->arg2;

    /* Check file mutex */
    if (!checkFileAccess(state, resolved_arg1)) {
        /* Resource not available, set blocked flag */
        state->flags->blocked = BLOCKED_FILE;
        strcpy(state->flags->blocked_file, resolved_arg1);
        result.blocked = 1;
        result.code = SYSCALL_BLOCKED;
        return result;
    }

    /* Write file using emulator API */
    int write_result = write_raw_data(resolved_arg1, resolved_arg2, strlen(resolved_arg2));

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

    int pid = getActivePCB(emu)->ProcessID;
    int implicit_lock = 0;

    /* Ensure we have console write access */
    if (state->mutexes->ConsoleWrite != -1 && state->mutexes->ConsoleWrite != pid) {
        state->flags->blocked = BLOCKED_CON_WRITE;
        result.blocked = 1;
        result.code = SYSCALL_BLOCKED;
        return result;
    }
    
    /* Implicitly acquire if free */
    if (state->mutexes->ConsoleWrite == -1) {
        state->mutexes->ConsoleWrite = pid;
        implicit_lock = 1;
    }

    /* Parse range arguments and print */
    char *v1 = get_variable(emu, pid, instr->arg1);
    const char *resolved_arg1 = (v1 && strlen(v1) > 0) ? v1 : instr->arg1;
    char *v2 = get_variable(emu, pid, instr->arg2);
    const char *resolved_arg2 = (v2 && strlen(v2) > 0) ? v2 : instr->arg2;

    int from = atoi(resolved_arg1);
    int to = atoi(resolved_arg2);
    
    char out_buf[256] = {0};
    int offset = 0;
    for (int i = from; i <= to && offset < 200; i++) {
        offset += snprintf(out_buf + offset, sizeof(out_buf) - offset, "%d ", i);
    }
    printToConsole("%s", out_buf);
    
    if (implicit_lock) {
        state->mutexes->ConsoleWrite = -1;
        state->flags->unblocked_con_write = 1;
    }
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

    int pid = getActivePCB(emu)->ProcessID;

    /* Check resource type and acquire appropriate mutex */
    switch (instr->resource) {
        case RES_USER_INPUT:
            if (state->mutexes->ConsoleRead == -1 || state->mutexes->ConsoleRead == pid) {
                state->mutexes->ConsoleRead = pid;
                state->flags->blocked = BLOCKED_NONE;
            } else {
                state->flags->blocked = BLOCKED_CON_READ;
                result.blocked = 1;
                result.code = SYSCALL_BLOCKED;
            }
            break;

        case RES_USER_OUTPUT:
            if (state->mutexes->ConsoleWrite == -1 || state->mutexes->ConsoleWrite == pid) {
                state->mutexes->ConsoleWrite = pid;
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

    int pid = getActivePCB(emu)->ProcessID;

    /* Release appropriate mutex based on resource type */
    switch (instr->resource) {
        case RES_USER_INPUT:
            if (state->mutexes->ConsoleRead == pid || state->mutexes->ConsoleRead == -1) {
                state->mutexes->ConsoleRead = -1;
                state->flags->unblocked_con_read = 1;
                state->flags->blocked = BLOCKED_NONE;
            } else {
                result.code = SYSCALL_ERROR;
            }
            break;

        case RES_USER_OUTPUT:
            if (state->mutexes->ConsoleWrite == pid || state->mutexes->ConsoleWrite == -1) {
                state->mutexes->ConsoleWrite = -1;
                state->flags->unblocked_con_write = 1;
                state->flags->blocked = BLOCKED_NONE;
            } else {
                result.code = SYSCALL_ERROR;
            }
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