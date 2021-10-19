CC = gcc 
CFLAGS =  -Wall -m64 -mavx2
LDFLAGS = -lpthread -lm
TARGS = bptree_test 

ifeq ($(mode), debug)
CFLAGS += -g
else
CFLAGS += -O3
endif

all: $(TARGS)

bptree_test: bptree.h bptree.c bptree_test.c
	$(CC) $(CFLAGS) bptree_test.c bptree.c -o bptree_test  $(LDFLAGS)

clean:
	rm -f *.o bptree_test *.asm 

clean_trees:
	rm -f trees/*.dot 