#ifndef PROJECT_ETHOS_GUI_H
#define PROJECT_ETHOS_GUI_H

#include "../kernal.h"

/* Start the GUI window in a separate thread */
void startGui(Emulator *emu, kernal_state *state);

/* Trigger a repaint of the GUI with current state */
void updateGui(void);

/* Block until the user clicks "Step" in the GUI */
void waitForGuiStep(void);

/* Block until the user finishes the configuration wizard in GUI mode */
void waitForGuiConfig(void);

/* Close the GUI window and clean up */
void stopGui(void);

#endif
