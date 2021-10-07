CC = gcc 
CFLAGS =  -Wall -m64 -mavx2
LDFLAGS = -lpthread -lm
TARGS = btree_test 

ifeq ($(mode), debug)
CFLAGS += -g
else
CFLAGS += -O3
endif

all: $(TARGS)

MurmurHash3.o: Murmurhash3/Murmurhash3.c
	$(CC) $(CFLAGS)  $< -o MurmurHash3.o -c 

btree.o: btree.c btree.h bitmap.h MurmurHash3.o
	$(CC) $(CFLAGS) $< -o btree.o -c 

btree_test:  btree_test.c btree.o MurmurHash3.o
	$(CC) $(CFLAGS) $< -o btree_test btree.o MurmurHash3.o $(LDFLAGS)

btree_test_asm:
	$(CC)  $(CFLAGS) -S btree.c -o btree_test.asm  -c

clean:
	rm -f *.o btree_test *.asm 

clean_trees:
	rm -f trees/*.dot