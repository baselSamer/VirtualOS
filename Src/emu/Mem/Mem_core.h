#ifndef PROJECT_ETHOS_MEM_CORE_H
#define PROJECT_ETHOS_MEM_CORE_H

/* Initializes all slots in the provided memory array to NULL. */
void initMem(void **mem);
/* Frees any non-NULL pointers contained within the provided memory array, then frees the array itself. */
void freeMem(void **mem);

#endif