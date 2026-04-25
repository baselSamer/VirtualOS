#include "emu/Core/Emulator.h"
#include "sys/kernal.h"

/* The main entry point of the simulator; initializes the emulator, starts the kernel, and cleans up upon exit. */
int main(void) {
    Emulator *emu = createEmulator();

    // Start the kernel
    start(emu);

    // Cleanup the emulator
    destroyEmulator(emu);

    return 0;
}