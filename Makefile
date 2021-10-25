CC = gcc 
CFLAGS =  -Wall -m64 -mavx2 -mbmi2
LDFLAGS = -lpthread -lm
TARGS = bptree_test 

ifeq ($(mode), debug)
CFLAGS += -g
else
CFLAGS += -O2
endif

all: $(TARGS)

bptree_test: src/*.c src/*.h
	$(CC) $(CFLAGS) src/*.c -o bptree_test  $(LDFLAGS)

bptree_asm: bptree.h bptree.c
	$(CC) $(CFLAGS) -S src/bptree.c -o bptree_test.asm  $(LDFLAGS)

clean:
	rm -rf *.o bptree_test *.asm  *.dSYM

clean_trees:
	rm -f trees/*.dot 