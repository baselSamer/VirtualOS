#include "ParserIntegration.h"
#include "../../../emu/Core/Logger.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Reads the file at filepath, parses its instructions, and returns a dynamically allocated ParsedScript structure. */
ParsedScript* parseScriptFromFile(const char *filepath) {
    ParsedScript *result = malloc(sizeof(ParsedScript));
    if (result == NULL) {
        return NULL;
    }

    FILE *file = fopen(filepath, "r");
    if (file == NULL) {
        emulatorLog("[PARSER] error=cannot_open_file path=%s", filepath);
        result->load_status = -1;
        result->instruction_count = 0;
        result->instructions = NULL;
        return result;
    }

    /* Count lines first */
    int line_count = 0;
    int ch;
    while ((ch = fgetc(file)) != EOF) {
        if (ch == '\n') {
            line_count++;
        }
    }
    if (ch != '\n') {
        line_count++;
    }
    rewind(file);

    /* Allocate instruction array */
    result->instructions = malloc(sizeof(Instruction) * line_count);
    if (result->instructions == NULL) {
        fclose(file);
        result->load_status = -1;
        result->instruction_count = 0;
        return result;
    }

    /* Parse each line */
    char line_buffer[512];
    int line_number = 0;
    int valid_instructions = 0;

    while (fgets(line_buffer, sizeof(line_buffer), file) != NULL) {
        /* Skip empty lines and comments */
        if (strlen(line_buffer) < 2 || line_buffer[0] == '#') {
            line_number++;
            continue;
        }

        Instruction parsed = parseInstruction(line_buffer, line_number);
        if (parsed.valid) {
            result->instructions[valid_instructions] = parsed;
            valid_instructions++;
        }
        line_number++;
    }

    fclose(file);
    result->instruction_count = valid_instructions;
    result->load_status = 0;

    emulatorLog("[PARSER] loaded file=%s instructions=%d", filepath, valid_instructions);

    return result;
}

/* Reads a script directly from a memory buffer, parsing its instructions into a dynamically allocated ParsedScript structure. */
ParsedScript* parseScriptFromBuffer(const char *buffer, size_t buffer_size) {
    ParsedScript *result = malloc(sizeof(ParsedScript));
    if (result == NULL) {
        return NULL;
    }

    if (buffer == NULL || buffer_size == 0) {
        result->load_status = -1;
        result->instruction_count = 0;
        result->instructions = NULL;
        return result;
    }

    /* Count newlines to estimate instruction count */
    int line_count = 0;
    for (size_t i = 0; i < buffer_size; i++) {
        if (buffer[i] == '\n') {
            line_count++;
        }
    }
    line_count++; /* Account for last line without newline */

    /* Allocate instruction array */
    result->instructions = malloc(sizeof(Instruction) * line_count);
    if (result->instructions == NULL) {
        result->load_status = -1;
        result->instruction_count = 0;
        return result;
    }

    /* Parse each line */
    char *buffer_copy = malloc(buffer_size + 1);
    if (buffer_copy == NULL) {
        free(result->instructions);
        result->load_status = -1;
        result->instruction_count = 0;
        return result;
    }
    strncpy(buffer_copy, buffer, buffer_size);
    buffer_copy[buffer_size] = '\0';

    char *saveptr = NULL;
    char *line = parser_strtok(buffer_copy, "\n", &saveptr);
    int line_number = 0;
    int valid_instructions = 0;

    while (line != NULL) {
        if (strlen(line) > 0 && line[0] != '#') {
            Instruction parsed = parseInstruction(line, line_number);
            if (parsed.valid) {
                result->instructions[valid_instructions] = parsed;
                valid_instructions++;
            }
        }
        line_number++;
        line = parser_strtok(NULL, "\n", &saveptr);
    }

    free(buffer_copy);
    result->instruction_count = valid_instructions;
    result->load_status = 0;

    emulatorLog("[PARSER] loaded_buffer size=%zu instructions=%d", buffer_size, valid_instructions);

    return result;
}

/* Frees a ParsedScript structure, including all instructions and arguments contained within it. */
void freeParsedScript(ParsedScript *script) {
    if (script == NULL) {
        return;
    }

    if (script->instructions != NULL) {
        for (int i = 0; i < script->instruction_count; i++) {
            freeInstruction(&script->instructions[i]);
        }
        free(script->instructions);
    }

    free(script);
}

/* Parses a file and loads its instructions securely into a Process Control Block (PCB). */
int loadScriptToProcess(Emulator *emu, PCB *pcb, const char *filepath) {
    if (emu == NULL || pcb == NULL || filepath == NULL) {
        return -1;
    }

    ParsedScript *script = parseScriptFromFile(filepath);
    if (script == NULL || script->load_status != 0) {
        emulatorLog("[PARSER] error=cannot_load_script pid=%d path=%s", pcb->ProcessID, filepath);
        if (script != NULL) {
            freeParsedScript(script);
        }
        return -1;
    }

    /* Set process bounds based on instruction count */
    pcb->bounds[0] = 0;                          /* Start address */
    pcb->bounds[1] = pcb->bounds[0];             /* Current position */
    pcb->bounds[2] = script->instruction_count;  /* End address */
    pcb->bounds[3] = script->instruction_count;  /* Total size */
    pcb->pc = 0;                                 /* Reset pc */

    emulatorLog("[PARSER] loaded_process pid=%d instructions=%d", pcb->ProcessID, script->instruction_count);

    freeParsedScript(script);
    return 0;
}