#include "Mem.h"
#include "Mem_core.h"
#include "../Core/Emulator_core.h"
#include <stdlib.h>

/* Writes a data pointer to the specified index in the emulator's simulated memory array. */
void writeMem(Emulator *emu, int loc, void* data)
{
    getMEM(emu)[loc] = data;
}

/* Frees any memory allocated at the specified index in the memory array and sets it to NULL. */
void freeMemLoc(Emulator *emu, int loc)
{
    if (getMEM(emu)[loc] != NULL) {
        free(getMEM(emu)[loc]);
        getMEM(emu)[loc] = NULL;
    }
}

/* Returns the data pointer stored at the specified index in the emulator's memory array. */
void* readMem(Emulator *emu, int loc)
{
    return getMEM(emu)[loc];
}

/* Initializes all slots in the provided memory array to NULL. */
void initMem(void **mem)
{
    for (int i = 0; i < 40; i++) {
        mem[i] = NULL;
    }
}

/* Frees any non-NULL pointers contained within the provided memory array, then frees the array itself. */
void freeMem(void **mem)
{
    for (int i = 0; i < 40; i++) {
        if (mem[i] != NULL) {
            free(mem[i]);
            mem[i] = NULL;
        }
    }
    free(mem);
}