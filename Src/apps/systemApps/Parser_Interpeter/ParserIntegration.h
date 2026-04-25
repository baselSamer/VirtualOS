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

/* Reads the file at filepath, parses its instructions, and returns a dynamically allocated ParsedScript structure. */
ParsedScript* parseScriptFromFile(const char *filepath);

/* Reads a script directly from a memory buffer, parsing its instructions into a dynamically allocated ParsedScript structure. */
ParsedScript* parseScriptFromBuffer(const char *buffer, size_t buffer_size);

/* Frees a ParsedScript structure, including all instructions and arguments contained within it. */
void freeParsedScript(ParsedScript *script);

/* Parses a file and loads its instructions securely into a Process Control Block (PCB). */
int loadScriptToProcess(Emulator *emu, PCB *pcb, const char *filepath);

#endif