CC = gcc 
CFLAGS =  -Wall -m64 -mavx2 -mbmi2
LDFLAGS = -lpthread -lm
TARGS = bin/bptree_test 

ifeq ($(mode), debug)
CFLAGS += -g
else
CFLAGS += -O2
endif

all: $(TARGS)

bin/bptree.o: src/bptree.c src/free_queue.c
	$(CC) $(CFLAGS) -c src/bptree.c src/free_queue.c && mv *.o bin/

bin/bptree_test: bin/bptree.o test/bptree_test.c
	$(CC) $(CFLAGS) -I src bin/*.o test/bptree_test.c -o bin/bptree_test $(LDFLAGS)

bptree_asm: bptree.h bptree.c
	$(CC) $(CFLAGS) -S src/bptree.c -o bptree_test.asm $(LDFLAGS)

.PHONY: clean clean_trees

clean:
	rm -rf  *.dSYM bin/*

clean_trees:
	rm -f trees/*.dot 