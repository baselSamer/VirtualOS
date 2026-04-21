#ifndef PROJECT_ETHOS_PARSER_INTEGRATION_H
#define PROJECT_ETHOS_PARSER_INTEGRATION_H

#include "Parser.h"
#include "../../../sys/kernal.h"

/* Load script from file or memory */
typedef struct {
    Instruction *instructions;
    int instruction_count;
    int load_status;
} ParsedScript;

/* Parse script from file */
ParsedScript* parseScriptFromFile(const char *filepath);

/* Parse script from memory buffer */
ParsedScript* parseScriptFromBuffer(const char *buffer, size_t buffer_size);

/* Free parsed script */
void freeParsedScript(ParsedScript *script);

/* Load script into process context */
int loadScriptToProcess(Emulator *emu, PCB *pcb, const char *filepath);

#endif /* PROJECT_ETHOS_PARSER_INTEGRATION_H */