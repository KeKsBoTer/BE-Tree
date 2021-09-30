CC = cc
CFLAGS = -O2 -Wall
LDFLAGS = -lpthread -lm
TARGS = btree

all: $(TARGS)
btree: btree.c 
	$(CC) $(CFLAGS) $< -o btree $(LDFLAGS)

btree_debug: btree.c 
	$(CC) -g -Wall $< -o btree $(LDFLAGS)

btree_assembly: btree.c 
	$(CC) -S $(CFLAGS) $< -o btree.asm

clean:
	rm -f $(TARGS) btree.asm