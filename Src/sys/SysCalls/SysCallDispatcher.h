#ifndef PROJECT_ETHOS_SYSCALL_DISPATCHER_H
#define PROJECT_ETHOS_SYSCALL_DISPATCHER_H

#include "../kernal.h"
#include "../../apps/systemApps/Parser_Interpeter/Parser.h"
#include "../Resouces/Mutex.h"

/* Syscall return codes */
typedef enum {
    SYSCALL_SUCCESS = 0,
    SYSCALL_BLOCKED = 1,
    SYSCALL_INVALID_ARGS = 2,
    SYSCALL_NO_PERMISSION = 3,
    SYSCALL_NOT_FOUND = 4,
    SYSCALL_ERROR = 5
} SyscallResult;

/* Syscall result structure */
typedef struct {
    SyscallResult code;
    char *output;          /* Output from syscall (for print operations) */
    void *data;            
    int blocked;           /* 1 if operation was blocked */
} SyscallResultData;

/* Handler function prototype */
typedef SyscallResultData (*SyscallHandler)(
    Emulator *emu,
    kernal_state *state,
    const Instruction *instr
);

/* Dispatcher function */
/* Routes an incoming instruction to the appropriate syscall handler based on its opcode and returns the result. */
SyscallResultData dispatchSyscall(
    Emulator *emu,
    kernal_state *state,
    const Instruction *instr,
    int pid
);

/* Individual syscall handlers */
/* Handles the PRINT instruction, outputting a value or variable directly to the emulator console. */
SyscallResultData syscall_print(
    Emulator *emu,
    kernal_state *state,
    const Instruction *instr
);

/* Handles the ASSIGN instruction, copying raw values, user input, or file data into a specified variable. */
SyscallResultData syscall_assign(
    Emulator *emu,
    kernal_state *state,
    const Instruction *instr
);

/* Handles the READ_FILE instruction, validating the file lock and loading file contents into a target memory buffer. */
SyscallResultData syscall_readFile(
    Emulator *emu,
    kernal_state *state,
    const Instruction *instr
);

/* Handles the WRITE_FILE instruction, asserting file lock ownership and saving variable contents safely to disk. */
SyscallResultData syscall_writeFile(
    Emulator *emu,
    kernal_state *state,
    const Instruction *instr
);

/* Handles the PRINT_FROM_TO instruction, securely locking the console and printing integers within a specified range. */
SyscallResultData syscall_printFromTo(
    Emulator *emu,
    kernal_state *state,
    const Instruction *instr
);

/* Handles the SEM_WAIT instruction, attempting to acquire exclusive access to a resource or blocking the process if unavailable. */
SyscallResultData syscall_semWait(
    Emulator *emu,
    kernal_state *state,
    const Instruction *instr
);

/* Handles the SEM_SIGNAL instruction, safely releasing a held resource lock and queuing waiting processes for wakeup. */
SyscallResultData syscall_semSignal(
    Emulator *emu,
    kernal_state *state,
    const Instruction *instr
);

/* Utility functions */
/* Frees any dynamically allocated output or data buffers contained within a SyscallResultData struct. */
void freeSyscallResult(SyscallResultData *result);

#endif