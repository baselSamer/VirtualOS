#include "Parser.h"
#include "../../../emu/Core/Logger.h"
#include "../../../emu/Console/Console.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Checks if a given line is NULL, empty, or consists entirely of whitespace characters. */
static int isEmptyInstructionLine(const char *line) {
    if (line == NULL) {
        return 1;
    }

    while (*line != '\0') {
        if (!isspace((unsigned char)*line)) {
            return 0;
        }
        line++;
    }

    return 1;
}

/* Converts a string representation of a resource to its corresponding ResourceType enum. */
static ResourceType parseResourceType(const char *resource_str) {
    if (resource_str == NULL) {
        return RES_INVALID;
    }
    
    if (strcmp(resource_str, "userInput") == 0) {
        return RES_USER_INPUT;
    } else if (strcmp(resource_str, "userOutput") == 0) {
        return RES_USER_OUTPUT;
    } else if (strcmp(resource_str, "file") == 0) {
        return RES_FILE;
    }
    
    return RES_INVALID;
}

/* Dynamically allocates memory and creates a duplicate of the given string. */
static char* string_dup(const char *str) {
    if (str == NULL) {
        return NULL;
    }
    char *dup = malloc(strlen(str) + 1);
    if (dup != NULL) {
        strcpy(dup, str);
    }
    return dup;
}

/* Parses a single text line into an Instruction struct, extracting its opcode and arguments. */
Instruction parseInstruction(const char *line, int line_number) {
    Instruction instr;
    instr.opcode = OP_INVALID;
    instr.arg1 = NULL;
    instr.arg2 = NULL;
    instr.arg3 = NULL;
    instr.resource = RES_INVALID;
    instr.valid = 0;
    instr.line_number = line_number;

    if (line == NULL || strlen(line) == 0) {
        emulatorLog("[PARSER] parse error line=%d token='empty'", line_number);
        return instr;
    }

    /* Create a working copy of the line */
    char *line_copy = malloc(strlen(line) + 1);
    if (line_copy == NULL) {
        return instr;
    }
    strcpy(line_copy, line);

    /* Get the opcode (first token) */
    char *saveptr = NULL;
    char *opcode_str = parser_strtok(line_copy, " \t\n\r", &saveptr);
    
    if (opcode_str == NULL) {
        emulatorLog("[PARSER] parse error line=%d token='%s'", line_number, opcode_str);
        free(line_copy);
        return instr;
    }

    /* Parse based on opcode */
    if (strcmp(opcode_str, "print") == 0) {
        instr.opcode = OP_PRINT;
        char *arg1 = parser_strtok(NULL, " \t\n\r", &saveptr);
        if (arg1 == NULL) {
            emulatorLog("[PARSER] parse error line=%d token='%s'", line_number, opcode_str);
            free(line_copy);
            return instr;
        }
        instr.arg1 = string_dup(arg1);
        instr.valid = 1;
    }
    else if (strcmp(opcode_str, "assign") == 0) {
        instr.opcode = OP_ASSIGN;
        char *arg1 = parser_strtok(NULL, " \t\n\r", &saveptr);
        char *arg2 = parser_strtok(NULL, " \t\n\r", &saveptr);
        if (arg1 == NULL || arg2 == NULL) {
            emulatorLog("[PARSER] parse error line=%d token='%s'", line_number, opcode_str);
            if (arg1) free(arg1);
            free(line_copy);
            return instr;
        }
        instr.arg1 = string_dup(arg1);
        instr.arg2 = string_dup(arg2);
        
        if (strcmp(arg2, "readFile") == 0) {
            char *arg3 = parser_strtok(NULL, " \t\n\r", &saveptr);
            if (arg3 != NULL) {
                instr.arg3 = string_dup(arg3);
            }
        }
        
        instr.valid = 1;
    }
    else if (strcmp(opcode_str, "writeFile") == 0) {
        instr.opcode = OP_WRITE_FILE;
        char *arg1 = parser_strtok(NULL, " \t\n\r", &saveptr);
        char *arg2 = parser_strtok(NULL, " \t\n\r", &saveptr);
        if (arg1 == NULL || arg2 == NULL) {
            emulatorLog("[PARSER] parse error line=%d token='%s'", line_number, opcode_str);
            free(line_copy);
            return instr;
        }
        instr.arg1 = string_dup(arg1);
        instr.arg2 = string_dup(arg2);
        instr.valid = 1;
    }
    else if (strcmp(opcode_str, "readFile") == 0) {
        instr.opcode = OP_READ_FILE;
        char *arg1 = parser_strtok(NULL, " \t\n\r", &saveptr);
        if (arg1 == NULL) {
            emulatorLog("[PARSER] parse error line=%d token='%s'", line_number, opcode_str);
            free(line_copy);
            return instr;
        }
        instr.arg1 = string_dup(arg1);
        instr.valid = 1;
    }
    else if (strcmp(opcode_str, "printFromTo") == 0) {
        instr.opcode = OP_PRINT_FROM_TO;
        char *arg1 = parser_strtok(NULL, " \t\n\r", &saveptr);
        char *arg2 = parser_strtok(NULL, " \t\n\r", &saveptr);
        if (arg1 == NULL || arg2 == NULL) {
            emulatorLog("[PARSER] parse error line=%d token='%s'", line_number, opcode_str);
            free(line_copy);
            return instr;
        }
        instr.arg1 = string_dup(arg1);
        instr.arg2 = string_dup(arg2);
        instr.valid = 1;
    }
    else if (strcmp(opcode_str, "semWait") == 0) {
        instr.opcode = OP_SEM_WAIT;
        char *resource = parser_strtok(NULL, " \t\n\r", &saveptr);
        if (resource == NULL) {
            emulatorLog("[PARSER] parse error line=%d token='%s'", line_number, opcode_str);
            free(line_copy);
            return instr;
        }
        instr.resource = parseResourceType(resource);
        if (instr.resource == RES_INVALID) {
            emulatorLog("[PARSER] parse error line=%d token='%s'", line_number, opcode_str);
            free(line_copy);
            return instr;
        }
        instr.arg1 = string_dup(resource);
        instr.valid = 1;
    }
    else if (strcmp(opcode_str, "semSignal") == 0) {
        instr.opcode = OP_SEM_SIGNAL;
        char *resource = parser_strtok(NULL, " \t\n\r", &saveptr);
        if (resource == NULL) {
            emulatorLog("[PARSER] parse error line=%d token='%s'", line_number, opcode_str);
            free(line_copy);
            return instr;
        }
        instr.resource = parseResourceType(resource);
        if (instr.resource == RES_INVALID) {
            emulatorLog("[PARSER] parse error line=%d token='%s'", line_number, opcode_str);
            free(line_copy);
            return instr;
        }
        instr.arg1 = string_dup(resource);
        instr.valid = 1;
    }
    else {
        emulatorLog("[PARSER] parse error line=%d token='%s'", line_number, opcode_str);
        free(line_copy);
        return instr;
    }

    free(line_copy);
    return instr;
}

/* Checks if the given instruction is marked as valid after parsing. */
int validateInstruction(Instruction *instr) {
    if (instr == NULL) {
        return 0;
    }
    return instr->valid;
}

/* Frees memory allocated for arguments within the instruction struct. */
void freeInstruction(Instruction *instr) {
    if (instr == NULL) {
        return;
    }
    if (instr->arg1 != NULL) {
        free(instr->arg1);
        instr->arg1 = NULL;
    }
    if (instr->arg2 != NULL) {
        free(instr->arg2);
        instr->arg2 = NULL;
    }
    if (instr->arg3 != NULL) {
        free(instr->arg3);
        instr->arg3 = NULL;
    }
}

#include "../../../sys/SysCalls/SysCallDispatcher.h"

/* Parses a line into an instruction, executes it via SysCallDispatcher, and returns whether process is blocked. */
int parse_and_execute(const char* code_line, int pid, kernal_state *state, Emulator *emu) {
    printToConsole("  | PCB: %-3d | Command: %s", pid, code_line);
    
    Instruction instr = parseInstruction(code_line, 0);
    if (!instr.valid) {
        if (state != NULL && emu != NULL && state->terminate_on_syntax_error && !isEmptyInstructionLine(code_line)) {
            PCB *active_pcb = getActivePCB(emu);
            if (active_pcb != NULL && active_pcb->ProcessID == pid) {
                active_pcb->state = TERMINATED;
                printToConsole("  | PCB: %-3d | *** TERMINATED (SYNTAX ERROR) ***", pid);
                emulatorLog("[PARSER] pid=%d action=terminate reason=syntax_error", pid);
                freeInstruction(&instr);
                return 1;
            }
        }

        freeInstruction(&instr);
        return 0; /* Invalid instructions just skip/fail silently for now */
    }
    
    SyscallResultData result = dispatchSyscall(emu, state, &instr, pid);
    
    int is_blocked = result.blocked;
    
    freeSyscallResult(&result);
    freeInstruction(&instr);
    
    return is_blocked;
}
