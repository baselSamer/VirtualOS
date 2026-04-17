# 🧪 Interpreter & Syscalls Test Execution Guide

This document explains how to **compile and run** the different test files in the project using `gcc`.


## 🔹 1. Test: `test_syscalls.c`

### 📌 Compile

gcc -I. -o Src/Tests/INTERPRETER_SYSCALLS/output/test_syscalls `
Src/Tests/INTERPRETER_SYSCALLS/test_syscalls.c `
Src/sys/SysCalls/SyscallDispatcher.c `
Src/sys/SysCalls/SyscallHandlers.c `
Src/sys/Parser/Parser.c `
Src/sys/flags.c `
Src/emu/Core/Emulator.c `
Src/emu/Core/Logger.c `
Src/emu/Console/Console.c `
Src/emu/Mem/Mem.c `
Src/emu/FIles/FIles.c

### ▶️ Run

./Src/Tests/INTERPRETER_SYSCALLS/output/test_syscalls


## 🔹 2. Test: `test_parser.c`

### 📌 Compile

gcc -I. -o Src/Tests/INTERPRETER_SYSCALLS/output/test_parser `
Src/Tests/INTERPRETER_SYSCALLS/test_parser.c `
Src/sys/Parser/Parser.c `
Src/sys/Parser/Tokenizer.c `
Src/sys/Parser/ParserIntegration.c `
Src/emu/Core/Logger.c `
Src/emu/Console/Console.c `
Src/emu/FIles/FIles.c

### ▶️ Run

./Src/Tests/INTERPRETER_SYSCALLS/output/test_parser


## 🔹 3. Test: `test_execution.c`

### 📌 Compile

gcc -I. -o Src/Tests/INTERPRETER_SYSCALLS/output/test_execution `
Src/Tests/INTERPRETER_SYSCALLS/test_execution.c `
Src/sys/flags.c `
Src/sys/SysCalls/SyscallDispatcher.c `
Src/sys/SysCalls/SyscallHandlers.c `
Src/sys/Parser/Parser.c `
Src/sys/Parser/Tokenizer.c `
Src/sys/Parser/ParserIntegration.c `
Src/emu/Core/Emulator.c `
Src/emu/Core/Logger.c `
Src/emu/Console/Console.c `
Src/emu/Mem/Mem.c `
Src/emu/FIles/FIles.c

### ▶️ Run

./Src/Tests/INTERPRETER_SYSCALLS/output/test_execution


## 🔹 4. Test: `test_all_interpreter_syscalls.c`

### 📌 Compile

gcc -I. -o Src/Tests/INTERPRETER_SYSCALLS/output/test_all_interpreter_syscalls `
Src/Tests/INTERPRETER_SYSCALLS/test_all_interpreter_syscalls.c `
Src/sys/SysCalls/SyscallDispatcher.c `
Src/sys/SysCalls/SyscallHandlers.c `
Src/sys/flags.c `
Src/sys/Parser/Parser.c `
Src/sys/Parser/Tokenizer.c `
Src/sys/Parser/ParserIntegration.c `
Src/sys/Excution.c `
Src/emu/Core/Emulator.c `
Src/emu/Core/Logger.c `
Src/emu/Console/Console.c `
Src/emu/Mem/Mem.c `
Src/emu/FIles/FIles.c

### ▶️ Run

./Src/Tests/INTERPRETER_SYSCALLS/output/test_all_interpreter_syscalls


# ✅ Notes

* Make sure the `output/` directory exists before compiling.
* Ensure all paths (especially `FIles`) match your project exactly.
* Run commands should be executed from the project root directory.