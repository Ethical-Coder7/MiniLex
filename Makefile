CC = gcc
CFLAGS = -Wall -Wextra -g

SRCS = main.c lexer_rules.c regex_parser.c nfa.c dfa.c tokenizer.c emit_c.c
OBJS = $(SRCS:.c=.o)
TARGET = minilex

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET) nfa.dot dfa.dot nfa.png dfa.png
