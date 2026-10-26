#!/bin/bash
# demo.sh — Reference presentation walkthrough script.
# Copy-paste lines sequentially during live demonstrations!

# 1. Clean build to show zero warnings live
make clean && make

# 2. Simple arithmetic tokenizer
./minilex examples/rules.lex "x1 + 42"

# 3. FLAGSHIP DEMO: Priority resolution (Must output [KEYWORD_IF "if"])
./minilex examples/mini_lang.lex "if (x == 10) { y = 20; }"

# 4. Maximal Munch 3-way overlap resolution
./minilex examples/overlap.lex "= == =>"

# 5. Dynamic generation against brand new rules
./minilex examples/verify.lex "<div> 123 <span>"

# 6. Lexical error handling and recovery (line/col error format)
# Outputs: Lexical error at position 2 (line 1, col 3): unexpected character '@'
./minilex examples/rules.lex "x @ y"

# 7. Empty-match rejection: Rules matching empty string are caught and rejected
echo "EMPTY = a*" > /tmp/bad_empty.lex && ./minilex /tmp/bad_empty.lex "abc"; echo "Exit: $?"; rm -f /tmp/bad_empty.lex

# 8. Gracefully handling malformed regexes inside the rule file itself
echo "BAD = (abc" > /tmp/bad1.lex && ./minilex /tmp/bad1.lex "abc"; echo "Exit: $?"; rm -f /tmp/bad1.lex

# 9. Token Positions: --positions flag shows line and column for every token
./minilex --positions examples/mini_lang.lex "if (x == 10) { y = 20; }"

# 10. %skip directive — consume whitespace silently
echo "NUM = [0-9]+" > /tmp/nows.lex
echo "ID = [a-zA-Z]+" >> /tmp/nows.lex
printf "WS = [ \t]+\n" >> /tmp/nows.lex
echo "%skip WS" >> /tmp/nows.lex
./minilex /tmp/nows.lex "abc 123"          # WS suppressed
./minilex --show-skipped /tmp/nows.lex "abc 123"   # WS restored
rm -f /tmp/nows.lex

# 11. DFA Minimization: Moore's Partition Refinement Algorithm
# Demonstrates state reduction using --stats and --minimize
# Flagship demo regex: T = a(b|c)*d|e(b|c)*d|f(b|c)*d
# Unminimized DFA: 13 states -> Minimized DFA: 3 states (77% reduction!)
./minilex --stats examples/minimization_demo.lex "abd"
./minilex --stats --minimize examples/minimization_demo.lex "abd"

# 12. --emit-c: Standalone C lexer emission (works with --minimize too!)
./minilex --minimize --emit-c /tmp/generated_lexer.c examples/mini_lang.lex "if (x == 10)"
gcc -o /tmp/generated_lexer /tmp/generated_lexer.c
/tmp/generated_lexer "if (x == 10)"      # byte-identical output to minilex
rm -f /tmp/generated_lexer /tmp/generated_lexer.c

# 13. Graphviz Visualizations (.dot to .png)
dot -Tpng nfa.dot -o nfa.png && dot -Tpng dfa.dot -o dfa.png

# 14. Reference Oracle and Differential Testing
python3 tests/oracle.py --fuzz ./minilex 100
python3 tests/diff_test.py 50

# 15. Closing with the automated test suite
./test_suite.sh --quick
