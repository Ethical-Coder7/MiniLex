#ifndef DFA_H
#define DFA_H

#include "nfa.h"

typedef struct DFAState {
    int id;
    int* nfa_states; // Array of NFA state IDs that make up this DFA state
    int nfa_states_count;
    
    // Array of transitions for all 256 possible byte values.
    // NULL if no transition for that character.
    struct DFAState* transitions[256];
    
    // 0 if not accepting. > 0 if accepting.
    // If multiple NFA states are accepting, the one with the lowest rule_id wins.
    int accept_rule_id;
} DFAState;

typedef struct {
    DFAState* start;
    DFAState** states;
    int num_states;
    int capacity;
} DFA;

// Convert NFA to DFA using Subset Construction (powerset construction)
DFA* build_dfa(NFA* nfa);

void free_dfa(DFA* dfa);

// Exports the DFA to a Graphviz dot file
void export_dfa_to_dot(DFA* dfa, const char* filename);

// Minimizes the DFA using Moore's partition refinement algorithm
DFA* minimize_dfa(DFA* dfa);

#endif
