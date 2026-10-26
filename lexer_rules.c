#include "lexer_rules.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_LINE_LEN 4096

// Helper to trim leading/trailing whitespace
static char* trim(char* str) {
    char* end;
    while(isspace((unsigned char)*str)) str++;
    if(*str == 0) return str;
    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

int parse_rules_file(const char* filename, Rule** out_rules, int* out_count) {
    FILE* fp = fopen(filename, "r");
    if (!fp) {
        perror("Failed to open rules file");
        return -1;
    }

    int capacity = 16;
    int count = 0;
    Rule* rules = malloc(capacity * sizeof(Rule));

    // Pending %skip names collected during first pass
    char** skip_names = NULL;
    int skip_count = 0;
    int skip_cap = 0;
    char** palindrome_names = NULL;
    int palindrome_count = 0;
    int palindrome_cap = 0;

    char line[MAX_LINE_LEN];
    while (fgets(line, sizeof(line), fp)) {
        char* trimmed = trim(line);
        // Skip empty lines or comments
        if (strlen(trimmed) == 0 || trimmed[0] == '#') {
            continue;
        }

        // Handle directives starting with %
        if (trimmed[0] == '%') {
            if (strncmp(trimmed, "%skip", 5) == 0 &&
                isspace((unsigned char)trimmed[5])) {
                char* name = trim(trimmed + 6);
                if (strlen(name) > 0) {
                    if (skip_count >= skip_cap) {
                        skip_cap = skip_cap == 0 ? 8 : skip_cap * 2;
                        skip_names = realloc(skip_names, skip_cap * sizeof(char*));
                    }
                    skip_names[skip_count++] = strdup(name);
                }
            } else if (strncmp(trimmed, "%palindrome", 11) == 0 &&
                       isspace((unsigned char)trimmed[11])) {
                char* name = trim(trimmed + 12);
                if (strlen(name) > 0) {
                    if (palindrome_count >= palindrome_cap) {
                        palindrome_cap = palindrome_cap == 0 ? 8 : palindrome_cap * 2;
                        palindrome_names = realloc(palindrome_names,
                                                   palindrome_cap * sizeof(char*));
                    }
                    palindrome_names[palindrome_count++] = strdup(name);
                }
            }
            continue;
        }

        // Parse "NAME = regex"
        char* eq = strchr(trimmed, '=');
        if (!eq) {
            fprintf(stderr, "Warning: Skipping invalid rule line (no '='): %s\n", trimmed);
            continue;
        }

        *eq = '\0'; // Split the string
        char* name = trim(trimmed);
        char* regex = trim(eq + 1);

        if (count >= capacity) {
            capacity *= 2;
            rules = realloc(rules, capacity * sizeof(Rule));
        }

        rules[count].name = strdup(name);
        rules[count].regex = strdup(regex);
        // rule_id starts at 1, so 0 can indicate "no rule matched"
        rules[count].rule_id = count + 1;
        rules[count].skip = 0;
        rules[count].palindrome = 0;
        count++;
    }

    fclose(fp);

    // Apply %skip directives collected above
    for (int s = 0; s < skip_count; s++) {
        int found = 0;
        for (int i = 0; i < count; i++) {
            if (strcmp(rules[i].name, skip_names[s]) == 0) {
                rules[i].skip = 1;
                found = 1;
                break;
            }
        }
        if (!found) {
            fprintf(stderr, "Warning: %%skip target '%s' not found in rules.\n",
                    skip_names[s]);
        }
        free(skip_names[s]);
    }
    free(skip_names);

    for (int p = 0; p < palindrome_count; p++) {
        int found = 0;
        for (int i = 0; i < count; i++) {
            if (strcmp(rules[i].name, palindrome_names[p]) == 0) {
                rules[i].palindrome = 1;
                found = 1;
                break;
            }
        }
        if (!found) {
            fprintf(stderr, "Warning: %%palindrome target '%s' not found in rules.\n",
                    palindrome_names[p]);
        }
        free(palindrome_names[p]);
    }
    free(palindrome_names);

    *out_rules = rules;
    *out_count = count;
    return 0;
}

void free_rules(Rule* rules, int count) {
    if (!rules) return;
    for (int i = 0; i < count; i++) {
        free(rules[i].name);
        free(rules[i].regex);
    }
    free(rules);
}
