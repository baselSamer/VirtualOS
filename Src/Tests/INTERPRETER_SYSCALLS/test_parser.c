/*
 * ═══════════════════════════════════════════════════════════════
 * TEST FILE: PARSER & INTERPRETER (TOKENIZER & INTEGRATION)
 * Location: Src/Tests/INTERPRETER_SYSCALLS/test_parser.c
 * Purpose: Test instruction parsing and script loading
 * 
 * COVERAGE: parseInstruction, tokenize, ParsedScript loading
 * ═══════════════════════════════════════════════════════════════
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Core Headers */
#include "../../apps/systemApps/Parser_Interpeter/Parser.h"
#include "../../apps/systemApps/Parser_Interpeter/ParserIntegration.h"
#include "../../emu/Core/Logger.h"

/* Test Statistics */
int total_tests = 0;
int passed_tests = 0;
int failed_tests = 0;

void print_test_header(const char *title) {
    printf("\n┌─────────────────────────────────────────────────────────┐\n");
    printf("│ %s\n", title);
    printf("└─────────────────────────────────────────────────────────┘\n");
}

void test_pass(const char *test_name) {
    printf("  ✓ [PASS] %s\n", test_name);
    passed_tests++;
    total_tests++;
}

void test_fail(const char *test_name, const char *reason) {
    printf("  ✗ [FAIL] %s\n         Reason: %s\n", test_name, reason);
    failed_tests++;
    total_tests++;
}

/* ═════════════════════════════════════════════════════════════════ */
/* TEST 1: PARSE PRINT INSTRUCTION */
/* ═════════════════════════════════════════════════════════════════ */
void test_parse_print() {
    print_test_header("TEST 1: Parse PRINT Instruction");
    
    Instruction instr = parseInstruction("print HelloWorld", 1);
    
    if (instr.valid == 0) {
        test_fail("Print instruction valid flag", "Instruction not valid");
        return;
    }
    test_pass("Print instruction valid flag");
    
    if (instr.opcode == OP_PRINT) {
        test_pass("Print instruction opcode (OP_PRINT)");
    } else {
        test_fail("Print opcode", "Not OP_PRINT");
    }
    
    if (instr.arg1 != NULL && strcmp(instr.arg1, "HelloWorld") == 0) {
        test_pass("Print arg1 value (HelloWorld)");
    } else {
        test_fail("Print arg1", "Incorrect or NULL");
    }
    
    freeInstruction(&instr);
}

/* ═════════════════════════════════════════════════════════════════ */
/* TEST 2: PARSE ASSIGN INSTRUCTION */
/* ═════════════════════════════════════════════════════════════════ */
void test_parse_assign() {
    print_test_header("TEST 2: Parse ASSIGN Instruction");
    
    Instruction instr = parseInstruction("assign counter 42", 2);
    
    if (instr.valid == 0) {
        test_fail("Assign instruction valid flag", "Instruction not valid");
        return;
    }
    test_pass("Assign instruction valid flag");
    
    if (instr.opcode == OP_ASSIGN) {
        test_pass("Assign instruction opcode (OP_ASSIGN)");
    } else {
        test_fail("Assign opcode", "Not OP_ASSIGN");
    }
    
    if (instr.arg1 != NULL && strcmp(instr.arg1, "counter") == 0) {
        test_pass("Assign arg1 (variable name)");
    } else {
        test_fail("Assign arg1", "Not 'counter'");
    }
    
    if (instr.arg2 != NULL && strcmp(instr.arg2, "42") == 0) {
        test_pass("Assign arg2 (value)");
    } else {
        test_fail("Assign arg2", "Not '42'");
    }
    
    freeInstruction(&instr);
}

/* ═════════════════════════════════════════════════════════════════ */
/* TEST 3: PARSE READ FILE INSTRUCTION */
/* ═════════════════════════════════════════════════════════════════ */
void test_parse_readfile() {
    print_test_header("TEST 3: Parse READFILE Instruction");
    
    Instruction instr = parseInstruction("readFile /etc/config.txt", 3);
    
    if (instr.valid == 0) {
        test_fail("ReadFile instruction valid flag", "Instruction not valid");
        return;
    }
    test_pass("ReadFile instruction valid flag");
    
    if (instr.opcode == OP_READ_FILE) {
        test_pass("ReadFile instruction opcode (OP_READ_FILE)");
    } else {
        test_fail("ReadFile opcode", "Not OP_READ_FILE");
    }
    
    if (instr.arg1 != NULL && strcmp(instr.arg1, "/etc/config.txt") == 0) {
        test_pass("ReadFile arg1 (file path)");
    } else {
        test_fail("ReadFile arg1", "Incorrect path");
    }
    
    freeInstruction(&instr);
}

/* ═════════════════════════════════════════════════════════════════ */
/* TEST 4: PARSE WRITE FILE INSTRUCTION */
/* ═════════════════════════════════════════════════════════════════ */
void test_parse_writefile() {
    print_test_header("TEST 4: Parse WRITEFILE Instruction");
    
    Instruction instr = parseInstruction("writeFile /tmp/output.txt TestData", 4);
    
    if (instr.valid == 0) {
        test_fail("WriteFile instruction valid flag", "Instruction not valid");
        return;
    }
    test_pass("WriteFile instruction valid flag");
    
    if (instr.opcode == OP_WRITE_FILE) {
        test_pass("WriteFile instruction opcode (OP_WRITE_FILE)");
    } else {
        test_fail("WriteFile opcode", "Not OP_WRITE_FILE");
    }
    
    if (instr.arg1 != NULL && strcmp(instr.arg1, "/tmp/output.txt") == 0) {
        test_pass("WriteFile arg1 (file path)");
    } else {
        test_fail("WriteFile arg1", "Incorrect path");
    }
    
    if (instr.arg2 != NULL && strcmp(instr.arg2, "TestData") == 0) {
        test_pass("WriteFile arg2 (content)");
    } else {
        test_fail("WriteFile arg2", "Incorrect content");
    }
    
    freeInstruction(&instr);
}

/* ═════════════════════════════════════════════════════════════════ */
/* TEST 5: PARSE PRINT FROM-TO INSTRUCTION */
/* ═════════════════════════════════════════════════════════════════ */
void test_parse_printfromto() {
    print_test_header("TEST 5: Parse PRINTFROMTO Instruction");
    
    Instruction instr = parseInstruction("printFromTo 1 10", 5);
    
    if (instr.valid == 0) {
        test_fail("PrintFromTo instruction valid flag", "Instruction not valid");
        return;
    }
    test_pass("PrintFromTo instruction valid flag");
    
    if (instr.opcode == OP_PRINT_FROM_TO) {
        test_pass("PrintFromTo instruction opcode (OP_PRINT_FROM_TO)");
    } else {
        test_fail("PrintFromTo opcode", "Not OP_PRINT_FROM_TO");
    }
    
    if (instr.arg1 != NULL && strcmp(instr.arg1, "1") == 0) {
        test_pass("PrintFromTo arg1 (start value)");
    } else {
        test_fail("PrintFromTo arg1", "Not '1'");
    }
    
    if (instr.arg2 != NULL && strcmp(instr.arg2, "10") == 0) {
        test_pass("PrintFromTo arg2 (end value)");
    } else {
        test_fail("PrintFromTo arg2", "Not '10'");
    }
    
    freeInstruction(&instr);
}

/* ═════════════════════════════════════════════════════════════════ */
/* TEST 6: PARSE SEMWAIT INSTRUCTION */
/* ═════════════════════════════════════════════════════════════════ */
void test_parse_semwait() {
    print_test_header("TEST 6: Parse SEMWAIT Instruction");
    
    Instruction instr = parseInstruction("semWait userOutput", 6);
    
    if (instr.valid == 0) {
        test_fail("SemWait instruction valid flag", "Instruction not valid");
        return;
    }
    test_pass("SemWait instruction valid flag");
    
    if (instr.opcode == OP_SEM_WAIT) {
        test_pass("SemWait instruction opcode (OP_SEM_WAIT)");
    } else {
        test_fail("SemWait opcode", "Not OP_SEM_WAIT");
    }
    
    if (instr.resource == RES_USER_OUTPUT) {
        test_pass("SemWait resource type (RES_USER_OUTPUT)");
    } else {
        test_fail("SemWait resource type", "Not RES_USER_OUTPUT");
    }
    
    freeInstruction(&instr);
}

/* ═════════════════════════════════════════════════════════════════ */
/* TEST 7: PARSE SEMSIGNAL INSTRUCTION */
/* ═════════════════════════════════════════════════════════════════ */
void test_parse_semsignal() {
    print_test_header("TEST 7: Parse SEMSIGNAL Instruction");
    
    Instruction instr = parseInstruction("semSignal file", 7);
    
    if (instr.valid == 0) {
        test_fail("SemSignal instruction valid flag", "Instruction not valid");
        return;
    }
    test_pass("SemSignal instruction valid flag");
    
    if (instr.opcode == OP_SEM_SIGNAL) {
        test_pass("SemSignal instruction opcode (OP_SEM_SIGNAL)");
    } else {
        test_fail("SemSignal opcode", "Not OP_SEM_SIGNAL");
    }
    
    if (instr.resource == RES_FILE) {
        test_pass("SemSignal resource type (RES_FILE)");
    } else {
        test_fail("SemSignal resource type", "Not RES_FILE");
    }
    
    freeInstruction(&instr);
}

/* ═════════════════════════════════════════════════════════════════ */
/* TEST 8: INVALID INSTRUCTION */
/* ═════════════════════════════════════════════════════════════════ */
void test_invalid_instruction() {
    print_test_header("TEST 8: Parse Invalid Instruction");
    
    Instruction instr = parseInstruction("invalid_op argument", 8);
    
    if (instr.valid == 0) {
        test_pass("Invalid instruction valid flag (correctly 0)");
    } else {
        test_fail("Invalid instruction valid flag", "Should be 0");
    }
    
    if (instr.opcode == OP_INVALID) {
        test_pass("Invalid instruction opcode (OP_INVALID)");
    } else {
        test_fail("Invalid instruction opcode", "Should be OP_INVALID");
    }
    
    freeInstruction(&instr);
}

/* ═════════════════════════════════════════════════════════════════ */
/* TEST 9: MISSING ARGUMENTS */
/* ═════════════════════════════════════════════════════════════════ */
void test_missing_arguments() {
    print_test_header("TEST 9: Parse Instruction with Missing Arguments");
    
    Instruction instr = parseInstruction("print", 9);
    
    if (instr.valid == 0) {
        test_pass("Missing arg instruction valid flag (correctly 0)");
    } else {
        test_fail("Missing arg instruction valid flag", "Should be 0");
    }
    
    freeInstruction(&instr);
}

/* ═════════════════════════════════════════════════════════════════ */
/* TEST 10: PARSE SCRIPT FROM BUFFER */
/* ═════════════════════════════════════════════════════════════════ */
void test_parse_buffer() {
    print_test_header("TEST 10: Parse Script from Buffer");
    
    const char *script = "print start\nassign x 5\nprint end\n";
    ParsedScript *parsed = parseScriptFromBuffer(script, strlen(script));
    
    if (parsed->load_status == 0) {
        test_pass("Script buffer loaded successfully (load_status = 0)");
    } else {
        test_fail("Script buffer load", "Load status not 0");
    }
    
    if (parsed->instruction_count == 3) {
        test_pass("Script instruction count (3 instructions)");
    } else {
        char buffer[128];
        snprintf(buffer, sizeof(buffer), "Expected 3, got %d", parsed->instruction_count);
        test_fail("Script instruction count", buffer);
    }
    
    freeParsedScript(parsed);
}

/* ═════════════════════════════════════════════════════════════════ */
/* TEST 11: PARSE SCRIPT WITH COMMENTS */
/* ═════════════════════════════════════════════════════════════════ */
void test_parse_comments() {
    print_test_header("TEST 11: Parse Script with Comments & Empty Lines");
    
    const char *script = "print step1\n# This is a comment\nassign y 20\n\nprint step3\n";
    ParsedScript *parsed = parseScriptFromBuffer(script, strlen(script));
    
    if (parsed->load_status == 0) {
        test_pass("Script with comments loaded successfully");
    } else {
        test_fail("Script with comments load", "Load status not 0");
    }
    
    if (parsed->instruction_count == 3) {
        test_pass("Comments and empty lines skipped correctly");
    } else {
        test_fail("Comment/empty line handling", 
                 "Expected 3 instructions (comments skipped)");
    }
    
    freeParsedScript(parsed);
}

/* ═════════════════════════════════════════════════════════════════ */
/* TEST 12: VALIDATE INSTRUCTION */
/* ═════════════════════════════════════════════════════════════════ */
void test_validate_instruction() {
    print_test_header("TEST 12: validateInstruction() Function");
    
    Instruction valid_instr = parseInstruction("print test", 12);
    
    if (validateInstruction(&valid_instr) == 1) {
        test_pass("validateInstruction returns 1 for valid");
    } else {
        test_fail("validateInstruction for valid", "Should return 1");
    }
    
    freeInstruction(&valid_instr);
    
    Instruction invalid_instr = parseInstruction("invalid_op", 12);
    
    if (validateInstruction(&invalid_instr) == 0) {
        test_pass("validateInstruction returns 0 for invalid");
    } else {
        test_fail("validateInstruction for invalid", "Should return 0");
    }
    
    freeInstruction(&invalid_instr);
}

/* ═════════════════════════════════════════════════════════════════ */
/* MAIN TEST RUNNER */
/* ═════════════════════════════════════════════════════════════════ */
int main() {
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════╗\n");
    printf("║                                                       ║\n");
    printf("║   INTERPRETER_SYSCALLS TEST SUITE                    ║\n");
    printf("║   Test File: Parser/Interpreter                      ║\n");
    printf("║                                                       ║\n");
    printf("╚═══════════════════════════════════════════════════════╝\n");

    /* Run all tests */
    test_parse_print();
    test_parse_assign();
    test_parse_readfile();
    test_parse_writefile();
    test_parse_printfromto();
    test_parse_semwait();
    test_parse_semsignal();
    test_invalid_instruction();
    test_missing_arguments();
    test_parse_buffer();
    test_parse_comments();
    test_validate_instruction();

    /* Final Summary */
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════╗\n");
    printf("║                   TEST SUMMARY                        ║\n");
    printf("╠═══════════════════════════════════════════════════════╣\n");
    printf("║ Total Tests:  %d                                      ║\n", total_tests);
    printf("║ Passed:       %d                                      ║\n", passed_tests);
    printf("║ Failed:       %d                                      ║\n", failed_tests);
    printf("╠═══════════════════════════════════════════════════════╣\n");
    
    if (failed_tests == 0) {
        printf("║ ✓ ALL TESTS PASSED                                    ║\n");
        printf("╚═══════════════════════════════════════════════════════╝\n");
        return 0;
    } else {
        printf("║ ✗ %d TEST(S) FAILED                                   ║\n", failed_tests);
        printf("╚═══════════════════════════════════════════════════════╝\n");
        return 1;
    }
}
