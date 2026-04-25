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

/* Allocates and initializes a new Emulator structure and its simulated memory. */
Emulator* createEmulator()
{
    Emulator *emu = (Emulator*)malloc(sizeof(Emulator));
    emu->activePCB = NULL;
    emu->mem = (void**)malloc(sizeof(void*) * 40); 
    initMem(emu->mem);
    return emu;
}

/* Frees memory allocated for an Emulator and its internal resources. */
void destroyEmulator(Emulator *emu)
{
    freeMem(emu->mem);
    free(emu);
}


/* Returns a pointer to the emulator's internal simulated memory array. */
void **getMEM(Emulator *emu)
{
    return emu->mem;
}

/* Retrieves the currently active Process Control Block (PCB) bound to the emulator. */
PCB* getActivePCB(Emulator *emu)
{
    return emu->activePCB;
}

/* Assigns a new active Process Control Block (PCB) to the emulator and returns it. */
PCB* setActivePCB(Emulator *emu, PCB *pcb)
{
    emu->activePCB = pcb;
    return emu->activePCB;
}

