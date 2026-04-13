// Memory.h
#ifndef PROJECT_ETHOS_MEMORY_H
#define PROJECT_ETHOS_MEMORY_H

#include "../../emu/Core/Emulator.h"

// The different types of data a memory slot can hold
typedef struct Variable {
    char name[32];
    char value[64];
} Variable;

typedef struct Instruction {
    char code_line[64];
} Instruction;

// The Universal Box that goes into the 40-slot array
typedef struct MemoryWord {
    int type; // 0 = PCB, 1 = Variable, 2 = Instruction
    union {
        PCB pcb_data;
        Variable var_data;
        Instruction inst_data;
    } content;
} MemoryWord;

#endif