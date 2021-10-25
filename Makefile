CC = gcc 
CFLAGS =  -Wall -m64 -mavx2
LDFLAGS = -lpthread -lm
TARGS = bptree_test 

ifeq ($(mode), debug)
CFLAGS += -g
else
CFLAGS += -O2
endif

all: $(TARGS)

bptree_test: bptree.h bptree.c bptree_test.c
	$(CC) $(CFLAGS) bptree_test.c bptree.c StopWatch.c -o bptree_test  $(LDFLAGS)

bptree_asm: bptree.h bptree.c
	$(CC) $(CFLAGS) -S bptree.c -o bptree_test.asm  $(LDFLAGS)

clean:
	rm -rf *.o bptree_test *.asm  *.dSYM

clean_trees:
	rm -f trees/*.dot 