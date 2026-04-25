#ifndef PROJECT_ETHOS_EMULATOR_H
#define PROJECT_ETHOS_EMULATOR_H

typedef struct PCB {
    int ProcessID;
    enum State { CREATED, READY, RUNNING, BLOCKED, TERMINATED } state;
    int pc;
    int bounds[4]; 
} PCB;

typedef struct Emulator Emulator;

/* Allocates and initializes a new Emulator structure and its simulated memory. */
Emulator* createEmulator(void);
/* Frees memory allocated for an Emulator and its internal resources. */
void destroyEmulator(Emulator *emu);
/* Retrieves the currently active Process Control Block (PCB) bound to the emulator. */
PCB* getActivePCB(Emulator *emu);
/* Assigns a new active Process Control Block (PCB) to the emulator and returns it. */
PCB* setActivePCB(Emulator *emu, PCB *pcb);

#endif