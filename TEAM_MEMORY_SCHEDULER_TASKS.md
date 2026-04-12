# Team 1: Memory Management + Scheduler Tasks

## Mission
Build process memory placement, loading, and scheduling logic in the kernel while respecting mutex/flags behavior and emulator boundaries.

## Scope
Own these areas under Src/sys:
- Memory/
- Scheduler/ (current folder name in repo is Schduler)
- Any shared state extensions in kernal.h

Do not bypass emulator APIs for file/console/memory resource access.

## Required Inputs
- PCB format from Src/emu/Core/Emulator.h
- kernal_state from Src/sys/kernal.h
- Mutex/flags conventions from Src/sys/Resouces/Mutex.h and Src/sys/flags.c

## Mandatory Rules
1. Add any new global/system runtime state to kernal.h.
2. If scheduler selects a process for execution, ensure process is in memory first.
3. Observe flags every tick before state transitions.
4. Keep all cross-folder function declarations in headers.
5. Add logs for scheduling decisions and memory events.
6. Free temporary allocations; define ownership for persistent allocations.

## PDF Constraints To Enforce
1. Main memory is fixed to 40 words.
2. Process arrivals baseline: P1@0, P2@1, P3@4 (must be configurable).
3. Scheduler algorithms required:
- HRRN (required)
- RR (required), quantum default 2 instructions
- MLFQ (bonus component)
4. Output must include queue snapshots and swap-in/out process IDs.

## Task Breakdown

### A. Memory Model Definition
1. Define memory layout policy in a design note under Src/sys/Memory/.
2. Implement two-direction growth model (recommended):
- PCB metadata from low addresses upward.
- Program/data blocks from high addresses downward.
3. Add bounds validation helpers.
4. Add compaction or reclamation behavior (at minimum for terminated processes).

### B. Process Load/Unload
1. Implement process load routine:
- Allocate PCB entry.
- Place process image in memory.
- Set bounds and initial pc.
2. Implement process unload routine:
- Release image and metadata blocks.
- Transition PCB to TERMINATED and reclaim memory.

### B1. Program Loading When Memory Is Full
1. Add an admission path for process arrival when there is no contiguous/usable space.
2. Select a victim process to swap out (policy must be deterministic and documented).
3. Persist victim process memory image to disk page/swap file before reclaiming RAM.
4. Update victim PCB/state to indicate swapped-out residency.
5. Retry loading arriving process after reclamation.
6. If still not loadable, return a clear failure code and log the reason.

### B2. Page/Swap File Management
1. Define and implement swap file format for each process image (metadata + payload).
2. Track swap location per process in kernel state.
3. Implement APIs for:
- write process image to swap file,
- read process image back into memory,
- delete obsolete swap entries on process termination.
4. Add integrity checks for swap reads (size/bounds/state validation).
5. Protect swap operations with file mutex semantics where required.

### B3. On-Demand Load/Deload During Process Switch
1. At each scheduler selection, verify selected process residency in main memory.
2. If selected process is swapped out:
- swap in before execution,
- if needed swap out another process first.
3. Define explicit trigger rules for swap out during active-process change.
4. Ensure pc, bounds, and variables are preserved across swap out/in cycles.
5. Keep READY/BLOCKED semantics correct even when process is off-memory.
6. Log every swap-in and swap-out with pid, reason, and memory region.

### C. Scheduler Core
1. Implement scheduler tick entry point (called each loop).
2. Implement RR and HRRN as selectable policies.
3. Keep RR time-slice configurable (default 2 instructions).
4. Add MLFQ if assigned for bonus scoring.
5. Handle states correctly:
- READY -> RUNNING
- RUNNING -> READY/BLOCKED/TERMINATED
6. Integrate blocked-flag awareness.
7. Maintain current_tick_count updates and log each decision.

### D. Integration Hooks
1. Export scheduler function(s) in appropriate header.
2. Integrate with start_exution loop in Src/sys/Excution.c.
3. Ensure no direct file/console access from scheduler internals.

## Logging Requirements
Use emulatorLog with tags:
- [SCHED]
- [MEM]
- [SWAP]

Examples:
- [SCHED] tick=15 selected pid=4
- [MEM] loaded pid=4 base=120 limit=220
- [MEM] freed pid=4 bytes=100
- [SWAP] out pid=2 reason=no_memory file=swap_p2.bin
- [SWAP] in pid=2 reason=selected_to_run

## Test Plan
1. Unit-style tests:
- Allocation success/failure paths.
- Bounds checks.
- Scheduler state transitions.
2. Scenario tests:
- 3+ processes with different run lengths.
- Blocked process should not be selected.
- Terminated process memory reclaimed.
 - Process arrival when memory full triggers correct swap-out and load.
 - Swapped-out selected process is swapped-in before execution.
 - Context switches preserve process state across swap cycles.
3. Leak check:
- Validate no growth after repeated load/unload cycles.
4. Swap-file correctness:
- Validate swap file creation, load-back correctness, and cleanup on terminate.

## Deliverables
1. Scheduler source/header with deterministic behavior.
2. Memory manager source/header with load/unload APIs.
3. Updated kernal.h state model.
4. Test notes and sample logs.

## Done Criteria
- Scheduler selects valid in-memory process every tick.
- Flags and states are consistently enforced.
- Logging is present for all key transitions.
- No obvious memory leaks in repeat-cycle tests.
- Process loading works when memory is full via deterministic swap policy.
- Swap/page files are correctly written, read, and cleaned.
