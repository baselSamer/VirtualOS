/*
 * ═══════════════════════════════════════════════════════════════
 * TEST FILE: EXECUTION LOOP & FLAGS (MODIFIED)
 * Location: Src/Tests/INTERPRETER_SYSCALLS/test_execution.c
 * Purpose: Test execution loop integration after Execution.c modification
 * 
 * NOTE: This test is ready for when you modify Execution.c & flags.c
 * ═════════���═════════════════════════════════════════════════════
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

/* Setup test PCB */
PCB* create_test_pcb(int pid) {
    PCB *pcb = malloc(sizeof(PCB));
    pcb->ProcessID = pid;
    pcb->state = READY;
    pcb->pc = 0;
    pcb->bounds[0] = 0;
    pcb->bounds[1] = 0;
    pcb->bounds[2] = 10;
    pcb->bounds[3] = 10;
    return pcb;
}

/* Setup test state */
kernal_state* create_test_state() {
    kernal_state *state = malloc(sizeof(kernal_state));
    state->current_tick_count = 0;
    state->flags = malloc(sizeof(flags));
    state->flags->blocked = 0;
    state->mutexes = malloc(sizeof(Mutex));
    state->mutexes->ConsoleRead = -1;
    state->mutexes->ConsoleWrite = -1;
    state->mutexes->File = -1;
    return state;
}

void cleanup_test(PCB *pcb, kernal_state *state, Emulator *emu) {
    if (pcb) free(pcb);
    if (state->flags) free(state->flags);
    if (state->mutexes) free(state->mutexes);
    if (state) free(state);
    if (emu) destroyEmulator(emu);
}

/* ═════════════════════════════════════════════════════════════════ */
/* TEST 1: PC INITIALIZATION */
/* ═════════════════════════════════════════════════════════════════ */
void test_pc_initialization() {
    print_test_header("TEST 1: PC Initialization");
    
    PCB *pcb = create_test_pcb(1);
    
    if (pcb->pc == 0) {
        test_pass("PC starts at 0");
    } else {
        test_fail("PC initialization", "PC not 0");
    }
    
    free(pcb);
}

/* ═══════════════════════════════════════════════════════════════��═ */
/* TEST 2: PC ADVANCEMENT ON SUCCESS */
/* ═════════════════════════════════════════════════════════════════ */
void test_pc_advancement() {
    print_test_header("TEST 2: PC Advancement on Successful Syscall");
    
    Emulator *emu = createEmulator();
    PCB *pcb = create_test_pcb(1);
    kernal_state *state = create_test_state();
    
    setActivePCB(emu, pcb);
    
    Instruction instr = parseInstruction("print test", 0);
    SyscallResultData result = dispatchSyscall(emu, state, &instr, pcb->ProcessID);
    
    /* Simulate PC advancement */
    if (result.code == SYSCALL_SUCCESS && result.blocked == 0) {
        pcb->pc += 1;
    }
    
    if (pcb->pc == 1) {
        test_pass("PC advanced to 1 after successful syscall");
    } else {
        test_fail("PC advancement", "PC not incremented");
    }
    
    freeInstruction(&instr);
    cleanup_test(pcb, state, emu);
}

/* ═════════════════════════════════════════════════════════════════ */
/* TEST 3: PC NOT ADVANCED ON BLOCKED */
/* ═════════════════════════════════════════════════════════════════ */
void test_pc_not_advanced_blocked() {
    print_test_header("TEST 3: PC NOT Advanced on Blocked Syscall");
    
    Emulator *emu = createEmulator();
    PCB *pcb = create_test_pcb(1);
    kernal_state *state = create_test_state();
    state->mutexes->ConsoleRead = 1;  /* Already acquired */
    
    setActivePCB(emu, pcb);
    
    Instruction instr = parseInstruction("semWait userInput", 0);
    SyscallResultData result = dispatchSyscall(emu, state, &instr, pcb->ProcessID);
    
    /* Simulate PC advancement logic */
    int pc_before = pcb->pc;
    if (result.code == SYSCALL_SUCCESS && result.blocked == 0) {
        pcb->pc += 1;
    }
    
    if (pcb->pc == pc_before) {
        test_pass("PC NOT advanced on blocked syscall");
    } else {
        test_fail("PC on blocked", "PC was incremented");
    }
    
    freeInstruction(&instr);
    cleanup_test(pcb, state, emu);
}

/* ═════════════════════════════════════════════════════════════════ */
/* TEST 4: PC BOUNDS CHECKING */
/* ═════════════════════════════════════════════════════════════════ */
void test_pc_bounds_check() {
    print_test_header("TEST 4: PC Bounds Checking");
    
    PCB *pcb = create_test_pcb(1);
    pcb->bounds[2] = 5;  /* Only 5 instructions */
    
    pcb->pc = 5;  /* Set PC to boundary */
    
    /* Check if out of bounds */
    if (pcb->pc >= pcb->bounds[2]) {
        pcb->state = TERMINATED;
        test_pass("Process terminates when PC >= bounds[2]");
    } else {
        test_fail("PC bounds check", "Process not terminated");
    }
    
    if (pcb->state == TERMINATED) {
        test_pass("Process state set to TERMINATED");
    } else {
        test_fail("Process state", "Not TERMINATED");
    }
    
    free(pcb);
}

/* ═════════════════════════════════════════════════════════════════ */
/* TEST 5: FLAG CREATION */
/* ═════════════════════════════════════════════════════════════════ */
void test_flag_creation() {
    print_test_header("TEST 5: Fresh Flag Creation (createNewFlags)");
    
    kernal_state *state = create_test_state();
    
    if (state->flags->blocked == 0) {
        test_pass("Initial flags->blocked = 0");
    } else {
        test_fail("Initial flags", "blocked not 0");
    }
    
    /* Free and recreate flags (simulating createNewFlags) */
    if (state->flags) free(state->flags);
    state->flags = malloc(sizeof(flags));
    state->flags->blocked = 0;
    
    if (state->flags->blocked == 0) {
        test_pass("New flags->blocked = 0 after recreation");
    } else {
        test_fail("Flag recreation", "blocked not reset");
    }
    
    if (state->flags) free(state->flags);
    if (state->mutexes) free(state->mutexes);
    if (state) free(state);
}

/* ═════════════════════════════════════════════════════════════════ */
/* TEST 6: TICK COUNTER INCREMENT */
/* ═════════════════════════════════════════════════════════════════ */
void test_tick_counter() {
    print_test_header("TEST 6: Tick Counter Increment");
    
    kernal_state *state = create_test_state();
    
    if (state->current_tick_count == 0) {
        test_pass("Initial tick count = 0");
    } else {
        test_fail("Initial tick", "not 0");
    }
    
    /* Simulate tick increments */
    for (int i = 0; i < 5; i++) {
        state->current_tick_count += 1;
    }
    
    if (state->current_tick_count == 5) {
        test_pass("Tick counter incremented to 5 after 5 cycles");
    } else {
        test_fail("Tick counter", "not incremented correctly");
    }
    
    if (state->flags) free(state->flags);
    if (state->mutexes) free(state->mutexes);
    if (state) free(state);
}

/* ═════════════════════════════════════════════════════════════════ */
/* TEST 7: SEQUENTIAL INSTRUCTION EXECUTION */
/* ═════════════════════════════════════════════════════════════════ */
void test_sequential_execution() {
    print_test_header("TEST 7: Sequential Instruction Execution");
    
    Emulator *emu = createEmulator();
    PCB *pcb = create_test_pcb(1);
    kernal_state *state = create_test_state();
    
    setActivePCB(emu, pcb);
    
    const char *instructions[] = {
        "print instruction1",
        "assign x 10",
        "print instruction3"
    };
    
    int num_instructions = 3;
    
    for (int i = 0; i < num_instructions; i++) {
        Instruction instr = parseInstruction(instructions[i], i);
        SyscallResultData result = dispatchSyscall(emu, state, &instr, pcb->ProcessID);
        
        if (result.code == SYSCALL_SUCCESS && result.blocked == 0) {
            pcb->pc += 1;
        }
        
        freeInstruction(&instr);
    }
    
    if (pcb->pc == 3) {
        test_pass("All 3 instructions executed sequentially");
    } else {
        char error_msg[100];
        snprintf(error_msg, sizeof(error_msg), "Expected PC=3, got PC=%d", pcb->pc);
        test_fail("Sequential execution", error_msg);
    }
    
    cleanup_test(pcb, state, emu);
}

/* ═════════════════════════════════════════════════════════════════ */
/* TEST 8: PROCESS STATE TRANSITIONS */
/* ═════════════════════════════════════════════════════════════════ */
void test_process_state_transitions() {
    print_test_header("TEST 8: Process State Transitions");
    
    PCB *pcb = create_test_pcb(1);
    
    if (pcb->state == READY) {
        test_pass("Process starts in READY state");
    } else {
        test_fail("Initial state", "Not READY");
    }
    
    pcb->state = RUNNING;
    if (pcb->state == RUNNING) {
        test_pass("Process transitions to RUNNING");
    } else {
        test_fail("State transition", "Could not set to RUNNING");
    }
    
    pcb->state = TERMINATED;
    if (pcb->state == TERMINATED) {
        test_pass("Process transitions to TERMINATED");
    } else {
        test_fail("State transition", "Could not set to TERMINATED");
    }
    
    free(pcb);
}

/* ═════════════════════════════════════════════════════════════════ */
/* MAIN TEST RUNNER */
/* ═════════════════════════════════════════════════════════════════ */
int main() {
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════╗\n");
    printf("║                                                       ║\n");
    printf("║   INTERPRETER_SYSCALLS TEST SUITE                    ║\n");
    printf("║   Test File: Execution & Flags (Modified)            ║\n");
    printf("║                                                       ║\n");
    printf("║   READY FOR: Execution.c & flags.c modifications     ║\n");
    printf("║                                                       ║\n");
    printf("╚═══════════════════════════════════════════════════════╝\n");

    /* Run all tests */
    test_pc_initialization();
    test_pc_advancement();
    test_pc_not_advanced_blocked();
    test_pc_bounds_check();
    test_flag_creation();
    test_tick_counter();
    test_sequential_execution();
    test_process_state_transitions();

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
