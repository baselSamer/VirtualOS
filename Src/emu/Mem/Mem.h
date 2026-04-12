#ifndef PROJECT_ETHOS_MEM_H
#define PROJECT_ETHOS_MEM_H

typedef struct Emulator Emulator;

void writeMem(Emulator *emu, int loc, void* data);
void* readMem(Emulator *emu, int loc);
void freeMemLoc(Emulator *emu, int loc);

#endif