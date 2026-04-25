#ifndef PROJECT_ETHOS_MEM_H
#define PROJECT_ETHOS_MEM_H

typedef struct Emulator Emulator;

/* Writes a data pointer to the specified index in the emulator's simulated memory array. */
void writeMem(Emulator *emu, int loc, void* data);
/* Returns the data pointer stored at the specified index in the emulator's memory array. */
void* readMem(Emulator *emu, int loc);
/* Frees any memory allocated at the specified index in the memory array and sets it to NULL. */
void freeMemLoc(Emulator *emu, int loc);

#endif