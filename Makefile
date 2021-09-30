CC = cc 
CFLAGS = -Wall -mavx2 -mlzcnt
LDFLAGS = -lpthread -lm
TARGS = btree

all: $(TARGS)
btree: btree.c 
	$(CC) -O2 $(CFLAGS) $< -o btree $(LDFLAGS)

btree_debug: btree.c 
	$(CC) -g $(CFLAGS) $< -o btree $(LDFLAGS)

btree_assembly: btree.c 
	$(CC) -S $(CFLAGS) $< -o btree.asm

clean:
	rm -f $(TARGS) btree.asm