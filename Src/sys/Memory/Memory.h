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

/* Loads a process from a file into main memory, creating its PCB, variables, and instructions. */
int load_process_to_memory(Emulator *emu, const char* filepath, int new_pid, struct kernal_state *state);
/* Scans main memory for consecutive empty slots, returning the start index or -1 if none found. */
int find_empty_space(Emulator *emu, int required_slots);
/* Evicts a process (preferably blocked) from main memory to disk to free up space. */
int swap_out(Emulator *emu);
/* Retrieves a previously evicted process from disk and restores it to main memory. */
int swap_in(Emulator *emu, int target_pid);
/* Searches the emulator's memory space and returns a pointer to the PCB struct with the specified Process ID. */
PCB* findPCB_FromID(Emulator *emu, int id);

/* Variable accessor helpers */
/* Locates a process variable by name and updates its value in memory. */
int set_variable(Emulator *emu, int pid, const char *name, const char *value);
/* Locates a process variable by name and retrieves its current value. */
char* get_variable(Emulator *emu, int pid, const char *name);

#endif