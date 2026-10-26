#ifndef LEXER_RULES_H
#define LEXER_RULES_H

typedef struct {
    char* name;
    char* regex;
    int rule_id; // Lower rule_id = higher priority (tie-breaker for maximal munch)
    int skip;    // 1 if declared with %skip (consume but don't print)
    int palindrome; // 1 if declared with %palindrome (semantic relabeling)
} Rule;

// Parses a .lex file containing lines like "NAME = regex" and optional
// "%skip NAME" and "%palindrome NAME" directives. Returns 0 on success, -1 on error.
// Allocates an array of rules in *out_rules and sets *out_count.
int parse_rules_file(const char* filename, Rule** out_rules, int* out_count);

void free_rules(Rule* rules, int count);

#endif
