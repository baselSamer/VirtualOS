#ifndef PROJECT_ETHOS_PARSER_H
#define PROJECT_ETHOS_PARSER_H

#include <stddef.h>
#include <string.h>
#include "../../../sys/kernal.h"
#include "../../../emu/Core/Emulator.h"

/* Tokenizes a string similarly to strtok_r, maintaining state between calls securely across platforms. */
static inline char* parser_strtok(char *str, const char *delim, char **saveptr) {
#ifdef _WIN32
    char *start;
    char *end;

    if (saveptr == NULL || delim == NULL) {
        return NULL;
    }

    start = (str != NULL) ? str : *saveptr;
    if (start == NULL) {
        return NULL;
    }

    start += strspn(start, delim);
    if (*start == '\0') {
        *saveptr = start;
        return NULL;
    }

    end = start + strcspn(start, delim);
    if (*end != '\0') {
        *end = '\0';
        end++;
    }

    *saveptr = end;
    return start;
#else
    return strtok_r(str, delim, saveptr);
#endif
}

/* Opcode definitions for all supported instructions */
typedef enum {
    OP_PRINT,           /* print x */
    OP_ASSIGN,          /* assign x y */
    OP_WRITE_FILE,      /* writeFile x y */
    OP_READ_FILE,       /* readFile x */
    OP_PRINT_FROM_TO,   /* printFromTo x y */
    OP_SEM_WAIT,        /* semWait resource */
    OP_SEM_SIGNAL,      /* semSignal resource */
    OP_INVALID          /* Invalid/unknown instruction */
} Opcode;

/* Resource types for semaphore operations */
typedef enum {
    RES_USER_INPUT,
    RES_USER_OUTPUT,
    RES_FILE,
    RES_INVALID
} ResourceType;

/* Parsed instruction structure */
typedef struct {
    Opcode opcode;
    char *arg1;           
    char *arg2;           
    char *arg3;           
    ResourceType resource; 
    int valid;            
    int line_number;      
} Instruction;

/* Parser context structure */
typedef struct {
    char *script;
    size_t script_len;
    size_t current_pos;
    int current_line;
} ParserContext;

/* Function prototypes */

/* Parses a single text line into an Instruction struct, extracting its opcode and arguments. */
Instruction parseInstruction(const char *line, int line_number);

/* Frees memory allocated for arguments within the instruction struct. */
void freeInstruction(Instruction *instr);

/* Tokenizes an input string and returns a dynamically allocated token at the specified zero-based index. */
char* tokenize(const char *input, int token_index);

/* Checks if the given instruction is marked as valid after parsing. */
int validateInstruction(Instruction *instr);

/* Parses a line into an instruction, executes it via SysCallDispatcher, and returns whether process is blocked. */
int parse_and_execute(const char* code_line, int pid, kernal_state *state, Emulator *emu);

#endif /* PROJECT_ETHOS_PARSER_H */
