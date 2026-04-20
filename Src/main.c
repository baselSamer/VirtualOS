#include "emu/Core/Emulator.h"
#include "sys/kernal.h"

int main(void) {
    // Initialize the emulator
    Emulator *emu = createEmulator();

    // Start the kernel
    start(emu);

    // Cleanup the emulator
    destroyEmulator(emu);

    return 0;
}