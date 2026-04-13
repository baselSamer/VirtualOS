#ifndef PROJECT_ETHOS_EMULATOR_H
#define PROJECT_ETHOS_EMULATOR_H

typedef struct PCB {
    int ProcessID;
    enum State { CREATED, READY, RUNNING, BLOCKED, TERMINATED } state;
    int pc;
    int bounds[4]; //first two numbers: start-end of code      //last 2 numbers start-end of variable locations
} PCB;

typedef struct Emulator Emulator;

Emulator* createEmulator(void);
void destroyEmulator(Emulator *emu);
PCB* getActivePCB(Emulator *emu);
void setActivePCB(Emulator *emu, PCB *new_process);

#endif