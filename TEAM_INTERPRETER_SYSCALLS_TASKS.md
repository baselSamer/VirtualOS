# Team 2: Parser/Interpreter + System Calls Tasks

## Mission
Implement instruction parsing/execution and syscall handling per tick, including flag and resource-access correctness.

## Scope
Own these areas under Src:
- apps/systemApps (parser/interpreter and UI integration)
- sys/SysCalls/
- Integration points in sys/Excution.c

## Mandatory Notes to Follow
1. Parser/interpreter is called every tick.
2. Instruction to execute is addressed by active process pc.
3. During each system call, create new flags.
4. For resource syscalls, verify access rights/mutex ownership.
5. If resource is blocked or unavailable, set blocked flag true.

## PDF Syntax Requirements
Interpreter must support at minimum:
1. print x
2. assign x y
3. writeFile x y
4. readFile x
5. printFromTo x y
6. semWait resource
7. semSignal resource

Resource names used by semWait/semSignal:
- userInput
- userOutput
- file

## Mandatory Rules
1. All exported parser/syscall functions must be declared in headers.
2. Do not access resources directly if emulator APIs already exist.
3. Maintain process state transitions coherently with scheduler.
4. Log each instruction execution and syscall result.

## Task Breakdown

### A. Instruction Representation
1. Define instruction format and parser output structure.
2. Implement tokenizer/parser for current app scripts.
3. Validate malformed input handling.

### B. Tick Execution Contract
1. On each tick:
- Read active process and pc.
- Fetch instruction from process memory/bounds.
- Execute one instruction.
- Advance pc only on successful/defined semantics.
2. Keep pc within bounds; terminate or fault on out-of-bounds.

### C. Syscall Layer
1. Implement syscall dispatcher in sys/SysCalls/.
2. For every syscall:
- Call createNewFlags(state).
- Validate arguments.
- Check resource permissions/mutex status.
- Set blocked flag when unavailable.
3. Return result codes and side effects consistently.

Required syscall service coverage:
1. Disk file read
2. Disk file write
3. Screen print
4. User input
5. Memory read
6. Memory write

### D. Resource-aware Syscalls
1. File syscalls:
- Require file mutex acquisition/ownership.
- Use emulator file APIs only.
2. Console syscalls:
- Require read/write mutex ownership based on operation.
- Use emulator console APIs only.

### E. Error and State Handling
1. Define syscall failure modes.
2. For blocked operations:
- Set blocked flag.
- Avoid mutating pc as completed instruction unless policy says retry/advance.
3. For fatal instruction errors:
- Transition process to TERMINATED if required.

## Logging Requirements
Use emulatorLog with tags:
- [PARSER]
- [EXEC]
- [SYSCALL]

Examples:
- [EXEC] pid=3 pc=14 opcode=READ_FILE
- [SYSCALL] pid=3 call=READ_FILE blocked=1 path=/x
- [PARSER] parse error pid=2 pc=8 token='@'

## Test Plan
1. Parser tests:
- Valid script parsing.
- Invalid syntax rejection.
2. Tick tests:
- Correct single-instruction execution per tick.
- pc updates and bounds checks.
3. Syscall tests:
- Mutex-owned success cases.
- Blocked resource failure with blocked flag set.
- Argument validation failures.

## Deliverables
1. Parser/interpreter modules with documented instruction set.
2. Syscall dispatcher and handlers in sys/SysCalls.
3. Integration in execution loop.
4. Test scripts and logs proving blocked/non-blocked behavior.

## Done Criteria
- One instruction executes per tick from pc.
- Every syscall creates/updates flags correctly.
- Resource syscalls enforce mutex and blocked semantics.
- Logs cover parser, execution, and syscall outcomes.
