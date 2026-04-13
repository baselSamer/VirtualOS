#include "console.h"
#include "../../../emu/Console/Console.h"

int console() {
        printToConsole("Console app started. Please enter some input:");
        char buffer[256];
        readFromConsole("%255s", buffer);
        printToConsole("You entered: %s", buffer);
        return 0;
}