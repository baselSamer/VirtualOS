# Interpreter & Syscalls Logging Guide

## Overview
All logging uses the `emulatorLog()` function from `Src/emu/Core/Logger.h`. Logs are written to a file named "logs".

## Logging Tags

### Parser Logs
Tag: `[PARSER]`

**Format**: `[PARSER] parse error pid=X pc=Y line=Z token='T'`

Examples:
- `[PARSER] parse error pid=2 pc=8 line=8 token='@'`
- `[PARSER] parse error pid=3 line=5 token='print_missing_arg'`
- `[PARSER] loaded file=/path/to/script.txt instructions=25`
- `[PARSER] loaded_buffer size=512 instructions=12`
- `[PARSER] error=cannot_open_file path=/x`
- `[PARSER] error=allocation_failed`

### Execution Logs
Tag: `[EXEC]`

**Format**: `[EXEC] pid=X pc=Y opcode=OP [result=R]`

Examples:
- `[EXEC] pid=3 pc=14 opcode=READ_FILE`
- `[EXEC] pid=1 pc=5 opcode=PRINT result=0`
- `[EXEC] pid=2 pc=10 error=out_of_bounds`
- `[EXEC] pid=3 status=blocked`
- `[EXEC] pid=3 status=terminated`
- `[EXEC] pid=0 pc=0 opcode=UI`

### Syscall Logs
Tag: `[SYSCALL]`

**Format**: `[SYSCALL] pid=X call=SYSCALL_NAME [result] [blocked=B] [path=P]`

Examples:
- `[SYSCALL] pid=3 call=READ_FILE blocked=1 path=/x`
- `[SYSCALL] pid=2 call=WRITE_FILE blocked=0 path=/data.txt`
- `[SYSCALL] pid=1 call=PRINT blocked=0`
- `[SYSCALL] pid=3 call=ASSIGN blocked=0 var=x`
- `[SYSCALL] pid=2 call=SEM_WAIT blocked=1 resource=userOutput`
- `[SYSCALL] pid=1 call=SEM_SIGNAL blocked=0 resource=file`
- `[SYSCALL] pid=5 call=INVALID error=invalid_instruction`
- `[SYSCALL] pid=4 call=UNKNOWN error=opcode_not_found`

### Flag Logs
Tag: `[FLAGS]`

Examples:
- `[FLAGS] created tick=42`
- `[FLAGS] error=allocation_failed`

### Kernel Logs
Tag: `[KERNEL]`

Examples:
- `[KERNEL] Execution loop started`
- `[KERNEL] Process 3 terminated at tick 156`
- `[KERNEL_STARTUP] start() called`
- `[KERNEL_STARTUP] kernel state initialized (tick=0)`

## Log File Format
All logs are appended to a file named "logs" in the current working directory. Each log entry is on a new line with the following format:

```
[TAG] message content\n
```

## Best Practices

1. **Always include pid** when logging syscalls and execution
2. **Always include pc** when logging instruction execution
3. **Use blocked=1/0** to indicate blocked state in syscalls
4. **Use result codes** to indicate success/failure
5. **Quote problematic tokens** in parse errors
6. **Log file paths** for file operations
7. **Log resource names** for semaphore operations