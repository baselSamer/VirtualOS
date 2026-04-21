# Project-Ethos

Project-Ethos is a layered OS/emulator-style project with clear privilege boundaries between emulator resources, kernel logic, and user-level apps.

## Architecture Overview

The project is split into three major layers:

1. Emulator (emu, Ring 0)
- Owns and controls all external resource access:
	- Console I/O
	- File I/O
	- Emulator memory block
- Exposes safe access functions that kernel code uses.

2. Kernel (sys)
- Implements core system behavior:
	- Startup and execution loop
	- Scheduling (planned/in progress)
	- Memory management (planned/in progress)
	- Synchronization (mutex)
	- System calls (planned/in progress)

3. Apps layer
- systemApps: UI + parser/interpreter system components.
- User Apps: custom user programs interpreted/executed by the parser.

## Current Source Structure

Top-level source root:
- Src

Main areas:
- Src/emu
	- Console
	- Core
	- FIles
	- Mem
- Src/sys
	- Start.c
	- Excution.c
	- flags.c
	- kernal.h
	- Gui
	- Memory
	- Parser
	- Resouces
	- Scheduler
	- SysCalls
- Src/apps
	- systemApps
		- Console
		- Parser_Interpeter
	- User Apps

## Core Data Structures

### PCB (process control block)
Defined in Src/emu/Core/Emulator.h.

Fields:
- ProcessID
- state: CREATED, READY, RUNNING, BLOCKED, TERMINATED
- pc (program counter)
- bounds[4] (process memory bounds/metadata)

### kernal_state
Defined in Src/sys/kernal.h.

Fields:
- current_tick_count
- flags (tracks exact blocking resource: BlockType enum, unblock signaling)
- mutexes
- ready queues (general, L1, L2, L3)
- blocked queue (general tracking)

### Mutex
Defined in Src/sys/kernal.h and operated through Src/sys/Resouces/Mutex.h.

Fields:
- ConsoleRead (int flag)
- ConsoleWrite (int flag)
- input_queue (Console Read specific waiting array)
- output_queue (Console Write specific waiting array)
- file_mutexes (linked list composed of Nodes tracking file path, owner id, and a local file-specific blocked_queue)

## Emulator APIs (Current)

### Console
Defined in Src/emu/Console/Console.h.

- printToConsole(const char* format, ...)
	- Formatted console output.
- readFromConsole(const char* format, ...)
	- Formatted input using caller-provided storage.
- readFromConsoleAlloc(const char* format, void** out_ptr)
	- Allocates storage dynamically based on format and writes parsed value.
	- Caller must free out_ptr after use.

### Logger
Defined in Src/emu/Core/Logger.h.

- emulatorLog(const char* format, ...)
	- Appends one log line to file named logs in current working directory.

### Memory Access
Defined in Src/emu/Mem/Mem.h and Src/emu/Mem/Mem_core.h.

- writeMem(Emulator* emu, int loc, void* data)
- readMem(Emulator* emu, int loc)
- freeMemLoc(Emulator* emu, int loc)
- initMem(void** mem)
- freeMem(void** mem)

### File Access
Defined in Src/emu/FIles/Files.h.

- read_raw_data(const char* file_path, void** out_ptr)
- write_raw_data(const char* file_path, const void* data, size_t size)

### Emulator Core Access
Defined in Src/emu/Core/Emulator.h and Src/emu/Core/Emulator_core.h.

- createEmulator(void)
- destroyEmulator(Emulator* emu)
- getActivePCB(Emulator* emu)
- getMEM(Emulator* emu)

## Kernel APIs (Current)

Defined in Src/sys/kernal.h and Src/sys/Resouces/Mutex.h.

- start(Emulator* emu)
- start_exution(Emulator* emu, kernal_state* state)
- createNewFlags(kernal_state* state)

Mutex operations:
- File mutex: fileMutexTaken, hasFileMutex, acquireFileMutex, releaseFileMutex
- Console read mutex: ConsoleReadMutexTaken, hasConsoleReadMutex, acquireConsoleReadMutex, releaseConsoleReadMutex
- Console write mutex: ConsoleWriteMutexTaken, hasConsoleWriteMutex, acquireConsoleWriteMutex, releaseConsoleWriteMutex

## Team Split and Ownership

Three implementation teams:

1. Memory Management + Scheduler Team
2. Parser/Interpreter + System Calls Team
3. Mutex, Skeleton, Console, and General Help Team

Detailed task plans are provided in:
- TEAM_MEMORY_SCHEDULER_TASKS.md
- TEAM_INTERPRETER_SYSCALLS_TASKS.md
- TEAM_MUTEX_SKELETON_CONSOLE_TASKS.md

## Project Requirements (From Provided PDF)

### Required System Calls
The system must support calls for:
1. Read file data from disk.
2. Write text output to file on disk.
3. Print data on screen.
4. Take user text input.
5. Read from memory.
6. Write to memory.

### Memory Constraints
1. Main memory size is fixed at 40 memory words.
2. Memory must hold instructions, variables, and PCB entries.
3. Each process should assume space for 3 variables.
4. Processes must not access outside their allocated memory bounds.
5. If memory is insufficient for a new arriving process:
- swap an existing process out to disk,
- keep it in scheduler structures,
- swap it back when scheduled again.

### PCB Required Fields
1. Process ID
2. Process state
3. Program counter
4. Memory boundaries

### Required Programs / Syntax Support
Program syntax must support:
- print x
- assign x y (including input case)
- writeFile x y
- readFile x
- printFromTo x y
- semWait resource
- semSignal resource

### Mutex Requirements
Implement 3 mutexes for resources:
1. file
2. userInput
3. userOutput

If resource is not available, process should be blocked and inserted in:
- resource blocked queue,
- general blocked queue.

### Scheduler Requirements
Required algorithms:
1. HRRN (non-preemptive)
2. RR (preemptive), default quantum: 2 instructions/time slice
3. MLFQ (bonus/assignment component)

Default process arrivals in requirement sheet:
- P1 at t=0
- P2 at t=1
- P3 at t=4

### Required Runtime Output
System output must show:
1. Ready/blocked queues after scheduling events.
2. Currently executing process.
3. Currently executing instruction.
4. Memory display each clock cycle (human readable).
5. Process IDs when swapped in/out.
6. Disk-memory format representation.

Note: timings, order, and time-slice values are subject to change during evaluation.

## Engineering Rules

1. Any cross-module API must be declared in a header file.
2. Log meaningful operations with emulatorLog.
3. Test every feature you add.
4. Free every malloc/realloc allocation when ownership ends.
5. Keep functions as stateless as practical.
6. Most objects are pointer-passed: validate pointer inputs.
7. Do not modify unclear areas before understanding current behavior.

## Windows Environment Setup

### Prerequisites

Install:
- Visual Studio Code
- GCC toolchain for Windows (MinGW-w64 or equivalent)
- CMake (3.16+)
- Git

Ensure these are in PATH:
- gcc
- cmake
- git

### Clone and open

1. Clone repository.
2. Open folder in VS Code.

### Build in VS Code

Use build task:

1. Press Ctrl+Shift+B
2. Select C/C++: gcc.exe build project

This task compiles project sources and outputs:
- Src/main.exe

### Build from terminal (PowerShell)

Run from repository root:

gcc -fdiagnostics-color=always -g Src/main.c Src/emu/Console/Console.c Src/emu/Core/Emulator.c Src/emu/Core/Logger.c Src/emu/FIles/FIles.c Src/emu/Mem/Mem.c -o Src/main.exe

### Run

From repository root:

./Src/main.exe

### Logs

Runtime logs are appended to:
- logs

File location depends on current working directory when executable runs.

## Current Development Notes

- Kernel startup and mutex module already emit logs to logs.
- main.c currently acts as a test harness and can be replaced by runtime boot path when integration is ready.