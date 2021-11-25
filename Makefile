CC = gcc
CFLAGS =  -Wall 
LDFLAGS = -lpthread -lm
TARGS = bin/bptree_test 
INCLUDE = -I ./include

all: $(TARGS)
# all: CFLAGS+=-O2

debug: CFLAGS+=-g
debug: $(TARGS)

bin/bptree.o: include/bptree.h src/bptree.c 
	$(CC) $(CFLAGS) $(INCLUDE) -c src/bptree.c $(LDFLAGS) && mv *.o bin/

bin/bptree_test: bin/bptree.o test/bptree_test.c
	$(CC) $(CFLAGS) $(INCLUDE) bin/bptree.o test/bptree_test.c -o bin/bptree_test $(LDFLAGS)

bptree_asm: include/bptree.h src/bptree.c
	$(CC) $(CFLAGS) $(INCLUDE) -S src/bptree.c -o bptree_test.asm $(LDFLAGS)

.PHONY: clean clean_trees

clean:
	rm -rf  *.dSYM bin/*

clean_trees:
	rm -f trees/*.dot 