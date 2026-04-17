#ifndef PROJECT_ETHOS_PARSER_H
#define PROJECT_ETHOS_PARSER_H

#include <stddef.h>

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
    char *arg1;           /* First argument (variable name, file path, resource name, or value) */
    char *arg2;           /* Second argument (for operations requiring two arguments) */
    ResourceType resource; /* Resource type for sem operations */
    int valid;            /* Flag indicating if instruction was parsed successfully */
    int line_number;      /* Line number in script for error reporting */
} Instruction;

/* Parser context structure */
typedef struct {
    char *script;
    size_t script_len;
    size_t current_pos;
    int current_line;
} ParserContext;

/* Function prototypes */
Instruction parseInstruction(const char *line, int line_number);
void freeInstruction(Instruction *instr);
char* tokenize(const char *input, int token_index);
int validateInstruction(Instruction *instr);

#endif /* PROJECT_ETHOS_PARSER_H */