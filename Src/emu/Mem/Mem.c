#include "Mem.h"
#include "Mem_core.h"
#include "../Core/Emulator_core.h"
#include <stdlib.h>

void writeMem(Emulator *emu, int loc, void* data)
{
    getMEM(emu)[loc] = data;
}

void freeMemLoc(Emulator *emu, int loc)
{
    if (getMEM(emu)[loc] != NULL) {
        free(getMEM(emu)[loc]);
        getMEM(emu)[loc] = NULL;
    }
}

void* readMem(Emulator *emu, int loc)
{
    return getMEM(emu)[loc];
}

void initMem(void **mem)
{
    for (int i = 0; i < 40; i++) {
        mem[i] = NULL;
    }
}

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