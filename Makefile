CC = gcc 
CFLAGS =  -Wall -m64 -mavx2 -mbmi2
LDFLAGS = -lpthread -lm
TARGS = build/bptree_test 

ifeq ($(mode), debug)
CFLAGS += -g
else
CFLAGS += -O2
endif

all: $(TARGS)

build/bptree.o: src/bptree.c
	$(CC) $(CFLAGS) -c $< && mv *.o build/

build/bptree_test: build/bptree.o test/bptree_test.c
	$(CC) $(CFLAGS) -I src build/*.o test/bptree_test.c -o build/bptree_test $(LDFLAGS)

bptree_asm: bptree.h bptree.c
	$(CC) $(CFLAGS) -S src/bptree.c -o bptree_test.asm $(LDFLAGS)

.PHONY: clean clean_trees

clean:
	rm -rf  *.dSYM build/*

clean_trees:
	rm -f trees/*.dot 