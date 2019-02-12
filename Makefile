CC=gcc
CFLAGS = -I .
DEPS  = parse.h y.tab.h log/logging.h
OBJS = y.tab.o lex.yy.o parse.o echo_server.o log/logging.o
FLAGS = -g -Wall

default: all

all: echo_server client

lex.yy.c: lexer.l
	flex $^

y.tab.c: parser.y
	yacc -d --debug --verbose $^

%.o: %.c $(DEPS)
	$(CC) $(FLAGS) -c -o $@ $< $(CFLAGS) 

echo_server: $(OBJS)
	$(CC) -o $@ $^ $(FLAGS) $(CFLAGS)

client: client.c
	$(CC) $^ -o $@ $(FLAGS)

clean:
	rm -f *.o *~ lex.yy.c y.tab.c y.tab.h echo_server client

