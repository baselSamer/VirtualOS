#include "kernal.h"
#include "../emu/Core/Logger.h"
#include <stdlib.h>
#include <string.h>

int createNewFlags(kernal_state *state) {
    if (state == NULL) {
        return -1;
    }
    
    /* Free existing flags if any */
    if (state->flags != NULL) {
        free(state->flags);
    }
    
    /* Allocate new flags */
    flags *new_flags = malloc(sizeof(flags));
    if (new_flags == NULL) {
        emulatorLog("[FLAGS] error=allocation_failed");
        return -1;
    }
    
    /* Initialize flags */
    new_flags->blocked = BLOCKED_NONE;
    
    new_flags->unblocked_con_read = 0;
    new_flags->unblocked_con_write = 0;
    new_flags->unblocked_file = 0;
    
    state->flags = new_flags;
    
    emulatorLog("[FLAGS] created tick=%d", state->current_tick_count);
    
    return 0;
}