#include "dfa.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

// Dynamic set of NFA state IDs
typedef struct {
    int* ids;
    int count;
    int capacity;
} NFAStateSet;

static void add_to_set(NFAStateSet* set, int id) {
    // Keep it sorted and unique
    for (int i = 0; i < set->count; i++) {
        if (set->ids[i] == id) return;
        if (set->ids[i] > id) {
            if (set->count >= set->capacity) {
                set->capacity = set->capacity == 0 ? 16 : set->capacity * 2;
                set->ids = realloc(set->ids, set->capacity * sizeof(int));
            }
            memmove(&set->ids[i + 1], &set->ids[i], (set->count - i) * sizeof(int));
            set->ids[i] = id;
            set->count++;
            return;
        }
    }
    // Append to the end
    if (set->count >= set->capacity) {
        set->capacity = set->capacity == 0 ? 16 : set->capacity * 2;
        set->ids = realloc(set->ids, set->capacity * sizeof(int));
    }
    set->ids[set->count++] = id;
}

static bool sets_equal(const NFAStateSet* a, const NFAStateSet* b) {
    if (a->count != b->count) return false;
    for (int i = 0; i < a->count; i++) {
        if (a->ids[i] != b->ids[i]) return false;
    }
    return true;
}

static unsigned int hash_set(const NFAStateSet* set) {
    unsigned int h = 2166136261u;
    for (int i = 0; i < set->count; i++) {
        h = (h ^ (unsigned int)set->ids[i]) * 16777619u;
    }
    return h;
}

typedef struct HashNode {
    DFAState* dfa_state;
    struct HashNode* next;
} HashNode;

typedef struct {
    HashNode** buckets;
    int num_buckets;
    int count;
} StateHashTable;

static StateHashTable* create_hash_table(int initial_size) {
    StateHashTable* ht = calloc(1, sizeof(StateHashTable));
    ht->num_buckets = initial_size > 0 ? initial_size : 1024;
    ht->buckets = calloc(ht->num_buckets, sizeof(HashNode*));
    ht->count = 0;
    return ht;
}

static void resize_hash_table(StateHashTable* ht) {
    int new_size = ht->num_buckets * 2;
    HashNode** new_buckets = calloc(new_size, sizeof(HashNode*));
    
    for (int i = 0; i < ht->num_buckets; i++) {
        HashNode* curr = ht->buckets[i];
        while (curr) {
            HashNode* next = curr->next;
            NFAStateSet s = {
                .ids = curr->dfa_state->nfa_states,
                .count = curr->dfa_state->nfa_states_count,
                .capacity = curr->dfa_state->nfa_states_count
            };
            unsigned int idx = hash_set(&s) % (unsigned int)new_size;
            curr->next = new_buckets[idx];
            new_buckets[idx] = curr;
            curr = next;
        }
    }
    free(ht->buckets);
    ht->buckets = new_buckets;
    ht->num_buckets = new_size;
}

static DFAState* find_in_hash_table(StateHashTable* ht, const NFAStateSet* set) {
    unsigned int h = hash_set(set);
    unsigned int idx = h % (unsigned int)ht->num_buckets;
    HashNode* curr = ht->buckets[idx];
    while (curr) {
        NFAStateSet existing = {
            .ids = curr->dfa_state->nfa_states,
            .count = curr->dfa_state->nfa_states_count,
            .capacity = curr->dfa_state->nfa_states_count
        };
        if (sets_equal(set, &existing)) {
            return curr->dfa_state;
        }
        curr = curr->next;
    }
    return NULL;
}

static void insert_in_hash_table(StateHashTable* ht, DFAState* state, const NFAStateSet* set) {
    if (ht->count >= ht->num_buckets) {
        resize_hash_table(ht);
    }
    unsigned int h = hash_set(set);
    unsigned int idx = h % (unsigned int)ht->num_buckets;
    HashNode* node = malloc(sizeof(HashNode));
    node->dfa_state = state;
    node->next = ht->buckets[idx];
    ht->buckets[idx] = node;
    ht->count++;
}

static void free_hash_table(StateHashTable* ht) {
    if (!ht) return;
    for (int i = 0; i < ht->num_buckets; i++) {
        HashNode* curr = ht->buckets[i];
        while (curr) {
            HashNode* next = curr->next;
            free(curr);
            curr = next;
        }
    }
    free(ht->buckets);
    free(ht);
}

static DFAState* new_dfa_state(DFA* dfa, NFAStateSet* set, NFA* nfa) {
    if (dfa->num_states >= dfa->capacity) {
        dfa->capacity = dfa->capacity == 0 ? 64 : dfa->capacity * 2;
        dfa->states = realloc(dfa->states, dfa->capacity * sizeof(DFAState*));
    }
    DFAState* s = calloc(1, sizeof(DFAState));
    s->id = dfa->num_states;
    s->nfa_states_count = set->count;
    s->nfa_states = malloc(set->count * sizeof(int));
    memcpy(s->nfa_states, set->ids, set->count * sizeof(int));
    
    // Determine accept_rule_id
    // If multiple NFA states accept, lowest rule_id (highest priority) wins.
    int min_rule_id = -1;
    for (int i = 0; i < set->count; i++) {
        int nfa_id = set->ids[i];
        int r_id = nfa->states[nfa_id]->accept_rule_id;
        if (r_id > 0) {
            if (min_rule_id == -1 || r_id < min_rule_id) {
                min_rule_id = r_id;
            }
        }
    }
    s->accept_rule_id = (min_rule_id == -1) ? 0 : min_rule_id;
    
    dfa->states[dfa->num_states++] = s;
    return s;
}

// Compute epsilon closure of a single state (DFS)
static void epsilon_closure_dfs(NFA* nfa, int state_id, NFAStateSet* closure, bool* visited) {
    if (visited[state_id]) return;
    visited[state_id] = true;
    add_to_set(closure, state_id);
    
    NFAState* s = nfa->states[state_id];
    if (s->type == TRANS_EPSILON) {
        if (s->out1) epsilon_closure_dfs(nfa, s->out1->id, closure, visited);
        if (s->out2) epsilon_closure_dfs(nfa, s->out2->id, closure, visited);
    }
}

// Compute epsilon closure for a whole set of states
static void compute_epsilon_closure(NFA* nfa, NFAStateSet* set) {
    bool* visited = calloc(nfa->num_states, sizeof(bool));
    NFAStateSet new_set = {0};
    
    for (int i = 0; i < set->count; i++) {
        epsilon_closure_dfs(nfa, set->ids[i], &new_set, visited);
    }
    
    free(visited);
    free(set->ids);
    *set = new_set; // Replace old set with closure
}

// Compute the set of NFA states reachable from a set via character c
static void move(NFA* nfa, NFAStateSet* set, unsigned char c, NFAStateSet* out) {
    for (int i = 0; i < set->count; i++) {
        NFAState* s = nfa->states[set->ids[i]];
        if (s->out1) {
            bool matches = false;
            if (s->type == TRANS_CHAR && (unsigned char)s->c == c) matches = true;
            else if (s->type == TRANS_ANY && c != '\n') matches = true; // '.' usually doesn't match newline
            else if (s->type == TRANS_CHARCLASS) {
                int byte_idx = c / 8;
                int bit_idx = c % 8;
                if (s->bitmap[byte_idx] & (1 << bit_idx)) matches = true;
            }
            
            if (matches) {
                add_to_set(out, s->out1->id);
            }
        }
    }
}

DFA* build_dfa(NFA* nfa) {
    DFA* dfa = calloc(1, sizeof(DFA));
    
    NFAStateSet start_set = {0};
    add_to_set(&start_set, nfa->start->id);
    compute_epsilon_closure(nfa, &start_set);
    
    StateHashTable* ht = create_hash_table(1024);
    
    DFAState* dfa_start = new_dfa_state(dfa, &start_set, nfa);
    dfa->start = dfa_start;
    insert_in_hash_table(ht, dfa_start, &start_set);
    free(start_set.ids); // Allocated inside new_dfa_state, free original
    
    // We use a queue for unprocessed DFA states
    int head = 0;
    
    while (head < dfa->num_states) {
        DFAState* current = dfa->states[head++];
        
        NFAStateSet current_set = {0};
        current_set.count = current->nfa_states_count;
        current_set.capacity = current->nfa_states_count;
        current_set.ids = current->nfa_states;
        
        // For every possible character, compute the transition
        for (int c = 0; c < 256; c++) {
            NFAStateSet next_set = {0};
            move(nfa, &current_set, (unsigned char)c, &next_set);
            
            if (next_set.count > 0) {
                compute_epsilon_closure(nfa, &next_set);
                
                // Check if next_set is already in dfa via hash table
                DFAState* target = find_in_hash_table(ht, &next_set);
                
                if (!target) {
                    target = new_dfa_state(dfa, &next_set, nfa);
                    insert_in_hash_table(ht, target, &next_set);
                }
                
                current->transitions[c] = target;
            }
            free(next_set.ids);
        }
    }
    
    free_hash_table(ht);
    return dfa;
}

void free_dfa(DFA* dfa) {
    if (!dfa) return;
    for (int i = 0; i < dfa->num_states; i++) {
        free(dfa->states[i]->nfa_states);
        free(dfa->states[i]);
    }
    free(dfa->states);
    free(dfa);
}

void export_dfa_to_dot(DFA* dfa, const char* filename) {
    if (dfa->num_states > 300) {
        fprintf(stderr, "Note: DFA has > 300 states, skipping DOT export.\n");
        return;
    }
    
    FILE* fp = fopen(filename, "w");
    if (!fp) {
        perror("Failed to open DFA dot file");
        return;
    }
    
    fprintf(fp, "digraph DFA {\n");
    fprintf(fp, "  rankdir=LR;\n");
    fprintf(fp, "  node [shape = circle];\n");
    
    for (int i = 0; i < dfa->num_states; i++) {
        DFAState* s = dfa->states[i];
        if (s->accept_rule_id > 0) {
            fprintf(fp, "  %d [shape = doublecircle, label=\"%d (R%d)\"];\n", 
                    s->id, s->id, s->accept_rule_id);
        }
    }
    
    fprintf(fp, "  start [shape=none, label=\"\"];\n");
    fprintf(fp, "  start -> %d;\n", dfa->start->id);
    
    typedef struct {
        DFAState* target;
        unsigned char labels[256];
        int label_count;
    } TargetGroup;
    
    int* target_map = malloc(dfa->num_states * sizeof(int));
    for (int i = 0; i < dfa->num_states; i++) target_map[i] = -1;
    
    TargetGroup groups[256];
    
    // Group transitions by target in O(states * 256)
    for (int i = 0; i < dfa->num_states; i++) {
        DFAState* s = dfa->states[i];
        int num_groups = 0;
        
        for (int c = 0; c < 256; c++) {
            DFAState* target = s->transitions[c];
            if (!target) continue;
            
            int tid = target->id;
            int g = target_map[tid];
            if (g == -1) {
                g = num_groups++;
                groups[g].target = target;
                groups[g].label_count = 0;
                target_map[tid] = g;
            }
            groups[g].labels[groups[g].label_count++] = (unsigned char)c;
        }
        
        for (int g = 0; g < num_groups; g++) {
            DFAState* target = groups[g].target;
            int label_count = groups[g].label_count;
            
            fprintf(fp, "  %d -> %d [label=\"", s->id, target->id);
            if (label_count > 5) {
                fprintf(fp, "%d chars", label_count);
            } else {
                for (int k = 0; k < label_count; k++) {
                    unsigned char c = groups[g].labels[k];
                    if (c == '"' || c == '\\') fprintf(fp, "\\\\%c", c);
                    else if (c >= 32 && c <= 126) fprintf(fp, "%c", c);
                    else fprintf(fp, "\\\\x%02x", c);
                    if (k < label_count - 1) fprintf(fp, ", ");
                }
            }
            fprintf(fp, "\"];\n");
            
            target_map[target->id] = -1;
        }
    }
    
    free(target_map);
    fprintf(fp, "}\n");
    fclose(fp);
}

static DFA* s_min_dfa = NULL;
static const int* s_min_curr_block = NULL;

static int min_state_cmp(const void* a, const void* b) {
    int u = *(const int*)a;
    int v = *(const int*)b;
    if (s_min_curr_block[u] != s_min_curr_block[v]) {
        return s_min_curr_block[u] - s_min_curr_block[v];
    }
    for (int c = 0; c < 256; c++) {
        DFAState* tu = s_min_dfa->states[u]->transitions[c];
        DFAState* tv = s_min_dfa->states[v]->transitions[c];
        int bu = tu ? s_min_curr_block[tu->id] : -1;
        int bv = tv ? s_min_curr_block[tv->id] : -1;
        if (bu != bv) {
            return bu - bv;
        }
    }
    return 0;
}

DFA* minimize_dfa(DFA* dfa) {
    if (!dfa || dfa->num_states == 0) return NULL;
    int n = dfa->num_states;
    if (n == 1) {
        DFA* min_dfa = calloc(1, sizeof(DFA));
        min_dfa->num_states = 1;
        min_dfa->capacity = 1;
        min_dfa->states = calloc(1, sizeof(DFAState*));
        DFAState* s = calloc(1, sizeof(DFAState));
        s->id = 0;
        s->accept_rule_id = dfa->states[0]->accept_rule_id;
        for (int c = 0; c < 256; c++) {
            if (dfa->states[0]->transitions[c]) {
                s->transitions[c] = s;
            }
        }
        min_dfa->states[0] = s;
        min_dfa->start = s;
        return min_dfa;
    }

    int* curr_block = malloc(n * sizeof(int));
    int* next_block = malloc(n * sizeof(int));
    int* order = malloc(n * sizeof(int));

    // Initial partition: group strictly by accept_rule_id
    // Non-accepting states have accept_rule_id == 0; accepting states have > 0
    int* rule_map = malloc(n * sizeof(int));
    int num_blocks = 0;
    for (int i = 0; i < n; i++) {
        int r = dfa->states[i]->accept_rule_id;
        int found = -1;
        for (int k = 0; k < num_blocks; k++) {
            if (rule_map[k] == r) {
                found = k;
                break;
            }
        }
        if (found == -1) {
            found = num_blocks;
            rule_map[num_blocks++] = r;
        }
        curr_block[i] = found;
    }
    free(rule_map);

    s_min_dfa = dfa;
    s_min_curr_block = curr_block;

    // Moore partition refinement loop
    while (1) {
        for (int i = 0; i < n; i++) order[i] = i;
        qsort(order, n, sizeof(int), min_state_cmp);

        int count = 0;
        next_block[order[0]] = 0;
        for (int i = 1; i < n; i++) {
            if (min_state_cmp(&order[i], &order[i - 1]) != 0) {
                count++;
            }
            next_block[order[i]] = count;
        }
        int new_num_blocks = count + 1;
        if (new_num_blocks == num_blocks) {
            // Partition has converged
            break;
        }
        num_blocks = new_num_blocks;
        memcpy(curr_block, next_block, n * sizeof(int));
    }

    // Rebuild DFA: start state must have ID 0, with contiguous IDs for all states
    int start_block = curr_block[dfa->start->id];
    int* block_to_id = malloc(num_blocks * sizeof(int));
    for (int b = 0; b < num_blocks; b++) block_to_id[b] = -1;

    int* rep_state = malloc(num_blocks * sizeof(int));
    for (int b = 0; b < num_blocks; b++) rep_state[b] = -1;
    for (int i = 0; i < n; i++) {
        int b = curr_block[i];
        if (rep_state[b] == -1) rep_state[b] = i;
    }

    // BFS ordering from start_block to assign contiguous state IDs starting at 0
    int* queue = malloc(num_blocks * sizeof(int));
    int qhead = 0, qtail = 0;

    block_to_id[start_block] = 0;
    queue[qtail++] = start_block;
    int next_id = 1;

    while (qhead < qtail) {
        int b = queue[qhead++];
        int u = rep_state[b];
        for (int c = 0; c < 256; c++) {
            DFAState* tgt = dfa->states[u]->transitions[c];
            if (tgt) {
                int tb = curr_block[tgt->id];
                if (block_to_id[tb] == -1) {
                    block_to_id[tb] = next_id++;
                    queue[qtail++] = tb;
                }
            }
        }
    }
    // Any remaining blocks (if any)
    for (int b = 0; b < num_blocks; b++) {
        if (block_to_id[b] == -1) {
            block_to_id[b] = next_id++;
        }
    }
    free(queue);

    // Allocate minimized DFA
    DFA* min_dfa = calloc(1, sizeof(DFA));
    min_dfa->num_states = num_blocks;
    min_dfa->capacity = num_blocks;
    min_dfa->states = calloc(num_blocks, sizeof(DFAState*));

    for (int i = 0; i < num_blocks; i++) {
        DFAState* s = calloc(1, sizeof(DFAState));
        s->id = i;
        min_dfa->states[i] = s;
    }
    min_dfa->start = min_dfa->states[0];

    // Populate transitions and accept rules
    for (int b = 0; b < num_blocks; b++) {
        int nid = block_to_id[b];
        int u = rep_state[b];
        DFAState* s = min_dfa->states[nid];
        s->accept_rule_id = dfa->states[u]->accept_rule_id;

        for (int c = 0; c < 256; c++) {
            DFAState* tgt = dfa->states[u]->transitions[c];
            if (tgt) {
                int tb = curr_block[tgt->id];
                s->transitions[c] = min_dfa->states[block_to_id[tb]];
            } else {
                s->transitions[c] = NULL;
            }
        }
    }

    free(rep_state);
    free(block_to_id);
    free(curr_block);
    free(next_block);
    free(order);

    s_min_dfa = NULL;
    s_min_curr_block = NULL;

    return min_dfa;
}
