#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lexer_rules.h"
#include "nfa.h"
#include "dfa.h"
#include "tokenizer.h"
#include "emit_c.h"

#include <sys/stat.h>

static char* read_entire_file(const char* filename) {
    struct stat st;
    if (stat(filename, &st) != 0) return NULL;
    if (!S_ISREG(st.st_mode)) return NULL;
    
    FILE* fp = fopen(filename, "rb");
    if (!fp) return NULL;
    
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return NULL;
    }
    long size = ftell(fp);
    if (size < 0) {
        fclose(fp);
        return NULL;
    }
    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return NULL;
    }
    
    char* buf = malloc((size_t)size + 1);
    if (!buf) {
        fclose(fp);
        return NULL;
    }
    size_t read_bytes = fread(buf, 1, (size_t)size, fp);
    if (read_bytes != (size_t)size && ferror(fp)) {
        free(buf);
        fclose(fp);
        return NULL;
    }
    buf[read_bytes] = '\0';
    fclose(fp);
    return buf;
}

static void print_help(const char* prog) {
    printf("Usage: %s [options] <rules.lex> <input>\n", prog);
    printf("Options:\n");
    printf("  -s, --string          Treat input as a literal string (default: auto-detect file)\n");
    printf("  --emit-c <out.c>      Write a standalone, table-driven C lexer to <out.c>\n");
    printf("  --show-skipped        Print tokens matched by %%skip rules (default: suppress)\n");
    printf("  --positions           Append L:C to each token: [RULE \"text\" L:C]\n");
    printf("  --minimize            Minimize DFA using Moore partition refinement\n");
    printf("  --stats               Print rule/NFA/DFA state counts to stderr\n");
    printf("  --help                Show this help and exit\n");
}

int main(int argc, char** argv) {
    const char* rules_file = NULL;
    const char* input_arg = NULL;
    const char* emit_c_file = NULL;
    int force_string = 0;
    int show_skipped = 0;
    int print_positions = 0;
    int print_stats = 0;
    int minimize = 0;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--string") == 0) {
            force_string = 1;
        } else if (strcmp(argv[i], "--show-skipped") == 0) {
            show_skipped = 1;
        } else if (strcmp(argv[i], "--positions") == 0) {
            print_positions = 1;
        } else if (strcmp(argv[i], "--stats") == 0) {
            print_stats = 1;
        } else if (strcmp(argv[i], "--minimize") == 0) {
            minimize = 1;
        } else if (strcmp(argv[i], "--emit-c") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "--emit-c requires an output file argument.\n");
                return 1;
            }
            emit_c_file = argv[++i];
        } else if (strcmp(argv[i], "--help") == 0) {
            print_help(argv[0]);
            return 0;
        } else if (!rules_file) {
            rules_file = argv[i];
        } else if (!input_arg) {
            input_arg = argv[i];
        } else {
            print_help(argv[0]);
            return 1;
        }
    }
    
    if (!rules_file || !input_arg) {
        print_help(argv[0]);
        return 1;
    }
    
    char* input_str = NULL;
    if (force_string) {
        input_str = strdup(input_arg);
    } else {
        input_str = read_entire_file(input_arg);
        if (!input_str) {
            input_str = strdup(input_arg);
        }
    }
    
    if (!input_str) {
        fprintf(stderr, "Failed to allocate memory for input string.\n");
        return 1;
    }
    
    Rule* rules = NULL;
    int rule_count = 0;
    
    if (parse_rules_file(rules_file, &rules, &rule_count) != 0) {
        fprintf(stderr, "Failed to parse rules file.\n");
        free(input_str);
        return 1;
    }
    
    // 1. Build NFA via Thompson's Construction
    NFA* nfa = build_nfa(rules, rule_count);
    if (!nfa) {
        fprintf(stderr, "Failed to build NFA. Aborting.\n");
        free_rules(rules, rule_count);
        free(input_str);
        return 1;
    }
    export_nfa_to_dot(nfa, "nfa.dot");
    
    // 2. Build DFA via Subset Construction
    DFA* dfa = build_dfa(nfa);
    int dfa_states_before = dfa->num_states;
    if (minimize) {
        DFA* min_dfa = minimize_dfa(dfa);
        free_dfa(dfa);
        dfa = min_dfa;
    }
    export_dfa_to_dot(dfa, "dfa.dot");
    
    // 3. Optional: print stats
    if (print_stats) {
        if (minimize) {
            fprintf(stderr, "[Stats] Rules: %d, NFA states: %d, DFA states: %d (before minimization: %d, after: %d)\n",
                    rule_count, nfa->num_states, dfa->num_states, dfa_states_before, dfa->num_states);
        } else {
            fprintf(stderr, "[Stats] Rules: %d, NFA states: %d, DFA states: %d\n",
                    rule_count, nfa->num_states, dfa->num_states);
        }
    }

    // 4. Optional: emit standalone C lexer
    if (emit_c_file) {
        if (emit_c(dfa, rules, rule_count, emit_c_file, rules_file) != 0) {
            fprintf(stderr, "Warning: Failed to write C lexer to '%s'.\n", emit_c_file);
        }
    }

    // 5. Tokenize using Maximal Munch
    int err = tokenize(dfa, rules, rule_count, input_str,
                       print_positions, show_skipped);
    
    // 6. Cleanup
    free_dfa(dfa);
    free_nfa(nfa);
    free_rules(rules, rule_count);
    free(input_str);
    
    return err;
}
