# Team 3: Mutex, Skeleton, Console, and General Help Tasks

## Mission
Stabilize kernel skeleton/runtime glue, maintain mutex subsystem, and support console/resource infrastructure used by all teams.

## Scope
Own/assist these areas:
- sys/Resouces/
- sys/Start.c, sys/Excution.c (skeleton/runtime wiring)
- emu/Console/
- Cross-team helper integration and debugging support

## Mandatory Rules
1. Keep mutex semantics deterministic and race-safe by design.
2. Keep interfaces header-first for cross-module usage.
3. Add logs for all mutex acquire/release/blocked/fail events.
4. Avoid hidden global state unless clearly documented in kernal.h.
5. Provide helper utilities to other teams without breaking boundaries.

## Task Breakdown

### A. Mutex Subsystem Hardening
1. Review existing file + console mutex functions for edge cases:
- Null pointers
- Missing active PCB
- Re-acquire by same owner
- Release by non-owner
2. Add defensive checks and clear return semantics.
3. Ensure linked-list file mutex management has no leaks.
4. Ensure mutex resources exactly cover:
- file
- userInput
- userOutput
5. If a process requests a busy resource, enforce blocking semantics:
- insert process in resource blocked queue
- insert process in general blocked queue

### B. Kernel Skeleton Reliability
1. Improve startup and execution-loop scaffolding:
- Safe allocations and failure paths in startup.
- Controlled loop behavior for debug mode.
2. Add integration hooks for scheduler and interpreter teams.
3. Ensure blocked flags and mutex states are visible in logs.

### C. Console Utility Support
1. Keep console API stable and documented.
2. Maintain dynamic allocation input path (readFromConsoleAlloc).
3. Add tests for console edge cases:
- Empty/whitespace inputs
- Invalid numeric conversion
- Allocation failures (where possible)

### D. General Support Role
1. Help teams integrate with emulatorLog.
2. Keep shared conventions document updated.
3. Assist in debugging integration failures between scheduler/interpreter/syscalls.

## Logging Requirements
Use emulatorLog with tags:
- [MUTEX]
- [KERNEL_STARTUP]
- [KERNEL_LOOP]
- [CONSOLE]

Examples:
- [MUTEX] acquireFileMutex pid=4 path='/tmp/a' result=blocked
- [KERNEL_STARTUP] state initialized tick=0
- [KERNEL_LOOP] tick=8 active_pid=2

## Test Plan
1. Mutex behavioral tests:
- Owner re-acquire should succeed.
- Non-owner release should fail.
- Competing acquire should block/fail as expected.
2. Skeleton tests:
- Startup handles allocation failure without crashing.
- Loop can execute deterministic test cycles.
3. Console tests:
- Formatted input/output correctness.
- Dynamic allocation path returns valid memory and values.

## Deliverables
1. Hardened mutex implementation and header docs.
2. Startup/loop skeleton updates with logging.
3. Console support updates and tests.
4. Shared support notes for other teams.

## Done Criteria
- Mutex operations are safe, leak-free, and logged.
- Kernel skeleton is robust and integration-ready.
- Console utilities are reliable and documented.
- Other teams can integrate without modifying emulator boundaries.
