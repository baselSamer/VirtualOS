#ifndef PROJECT_ETHOS_MEMORY_H
#define PROJECT_ETHOS_MEMORY_H
#include <../../emu/Core/Emulator.h>

typedef enum
{
    MEM_TYPE_PCB,
    MEM_TYPE_VARIABLE,
    MEM_TYPE_INSTRUCTION
} MemoryWordType;

struct Variable
{
    char name[32];
    char value[64];
};

struct Instruction
{
    char code_line[64];
};

struct MemoryWord
{
    MemoryWordType type;

    union
    {
        PCB pcb_data;
        struct Variable var_data;
        struct Instruction inst_data;
    } content;
};

#endif