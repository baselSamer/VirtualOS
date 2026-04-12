#include "kernal.h"
int createNewFlags(kernal_state *state) {
    flags *new_flags = malloc(sizeof(flags));
    new_flags->blocked = 0;
    state->flags = new_flags;
    return 0;
}