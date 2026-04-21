#include "Parser.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Tokenize a line into space-separated tokens */
char* tokenize(const char *input, int token_index) {
    if (input == NULL || token_index < 0) {
        return NULL;
    }

    char *line_copy = malloc(strlen(input) + 1);
    if (line_copy == NULL) {
        return NULL;
    }
    strcpy(line_copy, input);

    char *token = NULL;
    int current_index = 0;
    char *saveptr = NULL;

    /* Split by whitespace */
    for (char *p = line_copy; ; p = NULL) {
        token = parser_strtok(p, " \t\n\r", &saveptr);
        if (token == NULL) {
            free(line_copy);
            return NULL;
        }
        if (current_index == token_index) {
            char *result = malloc(strlen(token) + 1);
            if (result != NULL) {
                strcpy(result, token);
            }
            free(line_copy);
            return result;
        }
        current_index++;
    }
}

/* Helper function to trim whitespace from string */
static char* trim_whitespace(char *str) {
    if (str == NULL) {
        return NULL;
    }
    
    while (isspace((unsigned char)*str)) {
        str++;
    }
    
    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) {
        end--;
    }
    
    int len = end - str + 1;
    char *trimmed = malloc(len + 1);
    if (trimmed != NULL) {
        strncpy(trimmed, str, len);
        trimmed[len] = '\0';
    }
    return trimmed;
}

/* Helper function to count tokens in a line */
static int count_tokens(const char *line) {
    if (line == NULL) {
        return 0;
    }
    
    char *line_copy = malloc(strlen(line) + 1);
    if (line_copy == NULL) {
        return 0;
    }
    strcpy(line_copy, line);
    
    int count = 0;
    char *saveptr = NULL;
    char *token = parser_strtok(line_copy, " \t\n\r", &saveptr);
    
    while (token != NULL) {
        count++;
        token = parser_strtok(NULL, " \t\n\r", &saveptr);
    }
    
    free(line_copy);
    return count;
}