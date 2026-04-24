#ifndef PROJECT_ETHOS_MEMORY_H
#define PROJECT_ETHOS_MEMORY_H
#include "../../emu/Core/Emulator.h"

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

struct kernal_state;

int load_process_to_memory(Emulator *emu, const char* filepath, int new_pid, struct kernal_state *state);
int find_empty_space(Emulator *emu, int required_slots);
int swap_out(Emulator *emu);
int swap_in(Emulator *emu, int target_pid);
PCB* findPCB_FromID(Emulator *emu, int id);

/* Variable accessor helpers */
int set_variable(Emulator *emu, int pid, const char *name, const char *value);
char* get_variable(Emulator *emu, int pid, const char *name);

#endif