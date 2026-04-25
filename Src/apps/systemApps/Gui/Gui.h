#ifndef PROJECT_ETHOS_GUI_H
#define PROJECT_ETHOS_GUI_H

#include "../../../sys/kernal.h"

/* Spawns a background thread to launch and manage the graphical user interface. */
void startGui(Emulator *emu, kernal_state *state);

/* Safely signals the GUI window to repaint itself with the latest simulator state. */
void updateGui(void);

/* Pauses execution and waits until the user clicks the "Step" button in the GUI. */
void waitForGuiStep(void);

/* Halts simulator start up until GUI configuration screens have been completed. */
void waitForGuiConfig(void);

/* Gracefully closes the GUI window and cleans up allocated graphical resources. */
void stopGui(void);

#endif
