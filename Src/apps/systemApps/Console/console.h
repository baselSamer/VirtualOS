#ifndef PROJECT_ETHOS_SYSTEM_CONSOLE_APP_H
#define PROJECT_ETHOS_SYSTEM_CONSOLE_APP_H

#include "../../../emu/Core/Emulator.h"

struct kernal_state;

/* Handles user interaction to configure the scheduler algorithms and processes during startup. */
int console(Emulator *emu, struct kernal_state *state);

#endif