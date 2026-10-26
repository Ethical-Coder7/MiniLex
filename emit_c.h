#ifndef EMIT_C_H
#define EMIT_C_H

#include "dfa.h"
#include "lexer_rules.h"

// Write a standalone, table-driven C lexer to out_path, generated from dfa
// and rules. The generated file compiles with plain gcc and has its own main().
// rules_file is used only in a header comment. Returns 0 on success, -1 on error.
int emit_c(DFA* dfa, Rule* rules, int rule_count,
           const char* out_path, const char* rules_file);

#endif
