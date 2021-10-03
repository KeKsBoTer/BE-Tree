CC = cc 
CFLAGS =  -Wall -m64 # -mavx2 -mlzcnt
LDFLAGS = -lpthread -lm
TARGS = btree_test 

ifeq ($(mode), debug)
CFLAGS += -g
else
CFLAGS += -O2
endif

all: $(TARGS)

MurmurHash3.o: Murmurhash3/Murmurhash3.c
	$(CC)  $< -o MurmurHash3.o -c 

btree.o: btree.c MurmurHash3.o
	$(CC) $(CFLAGS) $< -o btree.o -c 

btree_test: btree_test.c btree.o MurmurHash3.o
	$(CC) $(CFLAGS) $< -o btree_test btree.o MurmurHash3.o $(LDFLAGS)

clean:
	rm -f *.o btree_test *.asm 

clean_trees:
	rm -f trees/*.dot