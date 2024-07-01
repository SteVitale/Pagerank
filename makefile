CC=gcc
CFLAGS=-std=c11 -Wall -g -O -pthread
LDLIBS=-lm -lrt -pthread


EXECS=pagerank 


all: $(EXECS) 

pagerank: pagerank.o x_func.o read.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)
	$(RM) *.o 

%.o: %.c x_func.h read.h
	$(CC) $(CFLAGS) -c $<

clean: 
	rm -f *.o $(EXECS)

