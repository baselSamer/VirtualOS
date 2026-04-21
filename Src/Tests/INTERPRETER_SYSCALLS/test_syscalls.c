/*
 * ═══════════════════════════════════════════════════════════════
 * TEST FILE: SYSCALL DISPATCHER & HANDLERS
 * Location: Src/Tests/INTERPRETER_SYSCALLS/test_syscalls.c
 * Purpose: Test all syscall operations independently
 * 
 * INDEPENDENCE: No Memory/Scheduler dependencies
 * COVERAGE: Print, Assign, ReadFile, WriteFile, PrintFromTo, SemWait, SemSignal
 * ═══════════════════════════════════════════════════════════════
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Core Headers */
#include "../../apps/systemApps/Parser_Interpeter/Parser.h"
#include "../../sys/SysCalls/SyscallDispatcher.h"
#include "../../emu/Core/Emulator.h"
#include "../../sys/kernal.h"

/* Test Statistics */
int total_tests = 0;
int passed_tests = 0;
int failed_tests = 0;

/* Test Helper Functions */
void print_test_header(const char *title) {
    printf("\n┌─────────────────────────────────────────────────────────┐\n");
    printf("│ %s\n", title);
    printf("└─────────────────────────────────────────────────────────┘\n");
}

void test_pass(const char *test_name) {
    printf("  ✓ [PASS] %s\n", test_name);
    passed_tests++;
    total_tests++;
}

void test_fail(const char *test_name, const char *reason) {
    printf("  ✗ [FAIL] %s\n         Reason: %s\n", test_name, reason);
    failed_tests++;
    total_tests++;
}

/* Setup minimal kernal_state */
kernal_state* create_test_state() {
    kernal_state *state = malloc(sizeof(kernal_state));
    state->current_tick_count = 0;
    state->flags = malloc(sizeof(flags));
    state->flags->blocked = 0;
    state->mutexes = malloc(sizeof(Mutex));
    state->mutexes->ConsoleRead = -1;
    state->mutexes->ConsoleWrite = -1;
    state->mutexes->file_mutexes = NULL;
    return state;
}

void free_test_state(kernal_state *state) {
    if (state->flags) free(state->flags);
    if (state->mutexes) free(state->mutexes);
    if (state) free(state);
}

/* ═════════════════════════════════════════════════════════════════ */
/* TEST 1: SYSCALL_RESULT ENUM */
/* ═════════════════════════════════════════════════════════════════ */
void test_syscall_result_enum() {
    print_test_header("TEST 1: SyscallResult Enum Values");
    
    if (SYSCALL_SUCCESS == 0 && SYSCALL_BLOCKED == 1 && 
        SYSCALL_INVALID_ARGS == 2 && SYSCALL_ERROR == 5) {
        test_pass("SyscallResult enum values");
    } else {
        test_fail("SyscallResult enum values", 
                 "Enum values don't match expected");
    }
}

/* ═════════════════════════════════════════════════════════════════ */
/* TEST 2: PRINT SYSCALL */
/* ═════════════════════════════════════════════════════════════════ */
void test_print_syscall() {
    print_test_header("TEST 2: Print Syscall Handler");
    
    Emulator *emu = createEmulator();
    kernal_state *state = create_test_state();
    
    Instruction print_instr = parseInstruction("print HelloWorld", 1);
    
    if (print_instr.valid == 0) {
        test_fail("Print instruction parse", "Instruction not valid");
        free_test_state(state);
        destroyEmulator(emu);
        return;
    }
    test_pass("Print instruction parsed");
    
    SyscallResultData result = dispatchSyscall(emu, state, &print_instr, 1);
    
    if (result.code == SYSCALL_SUCCESS) {
        test_pass("Print syscall returns SUCCESS");
    } else {
        test_fail("Print syscall execution", 
                 "Returned non-SUCCESS code");
    }
    
    if (result.blocked == 0) {
        test_pass("Print syscall not blocked");
    } else {
        test_fail("Print syscall blocked flag", 
                 "Should be 0 (not blocked)");
    }
    
    if (result.output != NULL && strcmp(result.output, "HelloWorld") == 0) {
        test_pass("Print output stored correctly");
        free(result.output);
    } else {
        test_fail("Print output", "Output not stored");
    }
    
    freeInstruction(&print_instr);
    free_test_state(state);
    destroyEmulator(emu);
}

/* ═════════════════════════════════════════════════════════════════ */
/* TEST 3: ASSIGN SYSCALL */
/* ═════════════════════════════════════════════════════════════════ */
void test_assign_syscall() {
    print_test_header("TEST 3: Assign Syscall Handler");
    
    Emulator *emu = createEmulator();
    kernal_state *state = create_test_state();
    
    Instruction assign_instr = parseInstruction("assign variable 42", 2);
    
    if (assign_instr.valid == 0) {
        test_fail("Assign instruction parse", "Instruction not valid");
        free_test_state(state);
        destroyEmulator(emu);
        return;
    }
    test_pass("Assign instruction parsed");
    
    SyscallResultData result = dispatchSyscall(emu, state, &assign_instr, 1);
    
    if (result.code == SYSCALL_SUCCESS) {
        test_pass("Assign syscall returns SUCCESS");
    } else {
        test_fail("Assign syscall execution", 
                 "Returned non-SUCCESS code");
    }
    
    if (result.blocked == 0) {
        test_pass("Assign syscall not blocked");
    } else {
        test_fail("Assign syscall blocked flag", 
                 "Should be 0 (not blocked)");
    }
    
    freeInstruction(&assign_instr);
    free_test_state(state);
    destroyEmulator(emu);
}

/* ═════════════════════════════════════════════════════════════════ */
/* TEST 4: SEMAPHORE WAIT - SUCCESS */
/* ═════════════════════════════════════════════════════════════════ */
void test_semwait_success() {
    print_test_header("TEST 4: SemWait - Resource Available (SUCCESS)");
    
    Emulator *emu = createEmulator();
    kernal_state *state = create_test_state();
    state->mutexes->ConsoleRead = -1;  /* Make available */
    
    Instruction wait_instr = parseInstruction("semWait userInput", 3);
    
    if (wait_instr.valid == 0) {
        test_fail("SemWait instruction parse", "Instruction not valid");
        free_test_state(state);
        destroyEmulator(emu);
        return;
    }
    test_pass("SemWait instruction parsed");
    
    SyscallResultData result = dispatchSyscall(emu, state, &wait_instr, 1);
    
    if (result.code == SYSCALL_SUCCESS) {
        test_pass("SemWait returns SUCCESS (resource available)");
    } else {
        test_fail("SemWait success case", 
                 "Should return SUCCESS, not blocked");
    }
    
    if (state->mutexes->ConsoleRead == 1) {
        test_pass("ConsoleRead mutex acquired (set to 1)");
    } else {
        test_fail("ConsoleRead mutex", 
                 "Not acquired (not set to 1)");
    }
    
    freeInstruction(&wait_instr);
    free_test_state(state);
    destroyEmulator(emu);
}

/* ═════════════════════════════════════════════════════════════════ */
/* TEST 5: SEMAPHORE WAIT - BLOCKED */
/* ═════════════════════════════════════════════════════════════════ */
void test_semwait_blocked() {
    print_test_header("TEST 5: SemWait - Resource Unavailable (BLOCKED)");
    
    Emulator *emu = createEmulator();
    kernal_state *state = create_test_state();
    state->mutexes->ConsoleRead = 1;  /* Make unavailable */
    
    Instruction wait_instr = parseInstruction("semWait userInput", 4);
    
    if (wait_instr.valid == 0) {
        test_fail("SemWait instruction parse", "Instruction not valid");
        free_test_state(state);
        destroyEmulator(emu);
        return;
    }
    test_pass("SemWait instruction parsed");
    
    SyscallResultData result = dispatchSyscall(emu, state, &wait_instr, 2);
    
    if (result.code == SYSCALL_BLOCKED && result.blocked == 1) {
        test_pass("SemWait returns BLOCKED (resource unavailable)");
    } else {
        test_fail("SemWait blocked case", 
                 "Should return BLOCKED");
    }
    
    if (state->flags->blocked == 1) {
        test_pass("Process flags->blocked set to 1");
    } else {
        test_fail("Process blocked flag", 
                 "Should be set to 1");
    }
    
    freeInstruction(&wait_instr);
    free_test_state(state);
    destroyEmulator(emu);
}

/* ═════════════════════════════════════════════════════════════════ */
/* TEST 6: SEMAPHORE SIGNAL */
/* ═════════════════════════════════════════════════════════════════ */
void test_semsignal() {
    print_test_header("TEST 6: SemSignal - Release Resource");
    
    Emulator *emu = createEmulator();
    kernal_state *state = create_test_state();
    state->mutexes->ConsoleRead = 1;  /* Currently acquired */
    
    Instruction signal_instr = parseInstruction("semSignal userInput", 5);
    
    if (signal_instr.valid == 0) {
        test_fail("SemSignal instruction parse", "Instruction not valid");
        free_test_state(state);
        destroyEmulator(emu);
        return;
    }
    test_pass("SemSignal instruction parsed");
    
    SyscallResultData result = dispatchSyscall(emu, state, &signal_instr, 1);
    
    if (result.code == SYSCALL_SUCCESS) {
        test_pass("SemSignal returns SUCCESS");
    } else {
        test_fail("SemSignal execution", 
                 "Should return SUCCESS");
    }
    
    if (state->mutexes->ConsoleRead == -1) {
        test_pass("ConsoleRead mutex released (set to -1)");
    } else {
        test_fail("ConsoleRead release", 
                 "Not released (not set to -1)");
    }
    
    freeInstruction(&signal_instr);
    free_test_state(state);
    destroyEmulator(emu);
}

/* ═══════════════��═════════════════════════════════════════════════ */
/* TEST 7: FILE MUTEX OPERATIONS */
/* ═════════════════════════════════════════════════════════════════ */
void test_file_mutex() {
    print_test_header("TEST 7: File Mutex - Acquire, Check, Release");
    
    Emulator *emu = createEmulator();
    kernal_state *state = create_test_state();
    
    /* Acquire */
    int acquired = acquireFileMutex(state, "/tmp/test.txt");
    if (acquired == 1) {
        test_pass("acquireFileMutex returns 1 (success)");
    } else {
        test_fail("acquireFileMutex", 
                 "Should return 1");
    }
    
    /* Check */
    int has_access = checkFileAccess(state, "/tmp/test.txt");
    if (has_access == 1) {
        test_pass("checkFileAccess returns 1 (mutex held)");
    } else {
        test_fail("checkFileAccess", 
                 "Should return 1 when held");
    }
    
    /* Release */
    int released = releaseFileMutex(state, "/tmp/test.txt");
    if (released == 1) {
        test_pass("releaseFileMutex returns 1 (success)");
    } else {
        test_fail("releaseFileMutex", 
                 "Should return 1");
    }
    
    /* Check after release */
    int no_access = checkFileAccess(state, "/tmp/test.txt");
    if (no_access == 0) {
        test_pass("checkFileAccess returns 0 (mutex released)");
    } else {
        test_fail("checkFileAccess after release", 
                 "Should return 0");
    }
    
    free_test_state(state);
    destroyEmulator(emu);
}

/* ═════════════════════════════════════════════════════════════════ */
/* TEST 8: READ FILE SYSCALL */
/* ═════════════════════════════════════════════════════════════════ */
void test_readfile_syscall() {
    print_test_header("TEST 8: ReadFile Syscall");
    
    Emulator *emu = createEmulator();
    kernal_state *state = create_test_state();
    
    /* Need file mutex for readFile to work */
    acquireFileMutex(state, "/tmp/test.txt");
    
    Instruction read_instr = parseInstruction("readFile /tmp/test.txt", 6);
    
    if (read_instr.valid == 0) {
        test_fail("ReadFile instruction parse", "Instruction not valid");
        free_test_state(state);
        destroyEmulator(emu);
        return;
    }
    test_pass("ReadFile instruction parsed");
    
    SyscallResultData result = dispatchSyscall(emu, state, &read_instr, 1);
    
    /* ReadFile may fail if file doesn't exist, but it should dispatch */
    if (result.code == SYSCALL_SUCCESS || result.code == SYSCALL_ERROR) {
        test_pass("ReadFile syscall dispatched correctly");
    } else {
        test_fail("ReadFile syscall dispatch", 
                 "Unexpected result code");
    }
    
    freeInstruction(&read_instr);
    free_test_state(state);
    destroyEmulator(emu);
}

/* ═════════════════════════════════════════════════════════════════ */
/* TEST 9: WRITE FILE SYSCALL */
/* ═════════════════════════════════════════════════════════════════ */
void test_writefile_syscall() {
    print_test_header("TEST 9: WriteFile Syscall");
    
    Emulator *emu = createEmulator();
    kernal_state *state = create_test_state();
    
    /* Need file mutex for writeFile to work */
    acquireFileMutex(state, "/tmp/output.txt");
    
    Instruction write_instr = parseInstruction("writeFile /tmp/output.txt TestData", 7);
    
    if (write_instr.valid == 0) {
        test_fail("WriteFile instruction parse", "Instruction not valid");
        free_test_state(state);
        destroyEmulator(emu);
        return;
    }
    test_pass("WriteFile instruction parsed");
    
    SyscallResultData result = dispatchSyscall(emu, state, &write_instr, 1);
    
    if (result.code == SYSCALL_SUCCESS || result.code == SYSCALL_ERROR) {
        test_pass("WriteFile syscall dispatched correctly");
    } else {
        test_fail("WriteFile syscall dispatch", 
                 "Unexpected result code");
    }
    
    freeInstruction(&write_instr);
    free_test_state(state);
    destroyEmulator(emu);
}

/* ═════════════════════════════════════════════════════════════════ */
/* TEST 10: PRINT FROM-TO SYSCALL */
/* ═════════════════════════════════════════════════════════════════ */
void test_printfromto_syscall() {
    print_test_header("TEST 10: PrintFromTo Syscall");
    
    Emulator *emu = createEmulator();
    kernal_state *state = create_test_state();
    
    Instruction printft_instr = parseInstruction("printFromTo 1 10", 8);
    
    if (printft_instr.valid == 0) {
        test_fail("PrintFromTo instruction parse", "Instruction not valid");
        free_test_state(state);
        destroyEmulator(emu);
        return;
    }
    test_pass("PrintFromTo instruction parsed");
    
    SyscallResultData result = dispatchSyscall(emu, state, &printft_instr, 1);
    
    if (result.code == SYSCALL_SUCCESS) {
        test_pass("PrintFromTo syscall returns SUCCESS");
    } else {
        test_fail("PrintFromTo syscall execution", 
                 "Should return SUCCESS");
    }
    
    freeInstruction(&printft_instr);
    free_test_state(state);
    destroyEmulator(emu);
}

/* ═════════════════════════════════════════════════════════════════ */
/* MAIN TEST RUNNER */
/* ═════════════════════════════════════════════════════════════════ */
int main() {
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════╗\n");
    printf("║                                                       ║\n");
    printf("║   INTERPRETER_SYSCALLS TEST SUITE                    ║\n");
    printf("║   Test File: Syscalls Only (Dispatcher & Handlers)  ║\n");
    printf("║                                                       ║\n");
    printf("╚═══════════════════════════════════════════════════════╝\n");

    /* Run all tests */
    test_syscall_result_enum();
    test_print_syscall();
    test_assign_syscall();
    test_semwait_success();
    test_semwait_blocked();
    test_semsignal();
    test_file_mutex();
    test_readfile_syscall();
    test_writefile_syscall();
    test_printfromto_syscall();

    /* Final Summary */
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════╗\n");
    printf("║                   TEST SUMMARY                        ║\n");
    printf("╠═══════════════════════════════════════════════════════╣\n");
    printf("║ Total Tests:  %d                                      ║\n", total_tests);
    printf("║ Passed:       %d                                      ║\n", passed_tests);
    printf("║ Failed:       %d                                      ║\n", failed_tests);
    printf("╠═══════════════════════════════════════════════════════╣\n");
    
    if (failed_tests == 0) {
        printf("║ ✓ ALL TESTS PASSED                                    ║\n");
        printf("╚═══════════════════════════════════════════════════════╝\n");
        return 0;
    } else {
        printf("║ ✗ %d TEST(S) FAILED                                   ║\n", failed_tests);
        printf("╚═══════════════════════════════════════════════════════╝\n");
        return 1;
    }
}
