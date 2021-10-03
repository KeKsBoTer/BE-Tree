CC = cc 
CFLAGS = -Wall -mavx2 -mlzcnt
LDFLAGS = -lpthread -lm
TARGS = btree_test 

ifeq ($(mode), debug)
CFLAGS += -g
endif

all: $(TARGS)

btree.o: btree.c
	$(CC) -O2 $(CFLAGS) $< -o btree.o -c 

btree_test: btree_test.c btree.o
	$(CC) -O2 $(CFLAGS) $< -o btree_test btree.o $(LDFLAGS)

clean:
	rm -f btree.o btree_test.o btree_test btree_test.asm