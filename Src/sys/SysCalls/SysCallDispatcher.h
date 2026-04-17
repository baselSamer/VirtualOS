#ifndef PROJECT_ETHOS_SYSCALL_DISPATCHER_H
#define PROJECT_ETHOS_SYSCALL_DISPATCHER_H

#include "../kernal.h"
#include "../Parser/Parser.h"

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
    void *data;            /* Generic data pointer for other results */
    int blocked;           /* 1 if operation was blocked */
} SyscallResultData;

/* Handler function prototype */
typedef SyscallResultData (*SyscallHandler)(
    Emulator *emu,
    kernal_state *state,
    const Instruction *instr
);

/* Dispatcher function */
SyscallResultData dispatchSyscall(
    Emulator *emu,
    kernal_state *state,
    const Instruction *instr,
    int pid
);

/* Individual syscall handlers */
SyscallResultData syscall_print(
    Emulator *emu,
    kernal_state *state,
    const Instruction *instr
);

SyscallResultData syscall_assign(
    Emulator *emu,
    kernal_state *state,
    const Instruction *instr
);

SyscallResultData syscall_readFile(
    Emulator *emu,
    kernal_state *state,
    const Instruction *instr
);

SyscallResultData syscall_writeFile(
    Emulator *emu,
    kernal_state *state,
    const Instruction *instr
);

SyscallResultData syscall_printFromTo(
    Emulator *emu,
    kernal_state *state,
    const Instruction *instr
);

SyscallResultData syscall_semWait(
    Emulator *emu,
    kernal_state *state,
    const Instruction *instr
);

SyscallResultData syscall_semSignal(
    Emulator *emu,
    kernal_state *state,
    const Instruction *instr
);

/* Utility functions */
void freeSyscallResult(SyscallResultData *result);
int checkFileAccess(kernal_state *state, const char *file_path);
int acquireFileMutex(kernal_state *state, const char *file_path);
int releaseFileMutex(kernal_state *state, const char *file_path);

#endif /* PROJECT_ETHOS_SYSCALL_DISPATCHER_H */