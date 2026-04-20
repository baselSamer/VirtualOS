#ifndef PROJECT_ETHOS_SYSTEM_CONSOLE_APP_H
#define PROJECT_ETHOS_SYSTEM_CONSOLE_APP_H

#include "../../../emu/Core/Emulator.h"

struct kernal_state;

int console(Emulator *emu, struct kernal_state *state);

#endif