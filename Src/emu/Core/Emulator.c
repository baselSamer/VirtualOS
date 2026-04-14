#include "Emulator.h"
#include "../Mem/Mem.h"
#include "Emulator_core.h"
#include "../Mem/Mem_core.h"
#include <stdlib.h>

struct Emulator
{
    PCB *activePCB;
    void** mem;
};

Emulator* createEmulator()
{
    Emulator *emu = (Emulator*)malloc(sizeof(Emulator));
    emu->activePCB = NULL;
    emu->mem = (void**)malloc(sizeof(void*) * 40); 
    initMem(emu->mem);
    return emu;
}

void destroyEmulator(Emulator *emu)
{
    freeMem(emu->mem);
    free(emu);
}


void **getMEM(Emulator *emu)
{
    return emu->mem;
}

PCB* getActivePCB(Emulator *emu)
{
    return emu->activePCB;
}

PCB* setActivePCB(Emulator *emu, PCB *pcb)
{
    emu->activePCB = pcb;
    return emu->activePCB;
}

