CC = cc
CFLAGS = -Wall
LDFLAGS = -lpthread -lm
TARGS = btree

all: $(TARGS)
btree: btree.c 
	$(CC) -g $(CFLAGS) $< -o btree $(LDFLAGS)

btree_assembly: btree.c 
	$(CC) -S $(CFLAGS) $< -o btree.asm

clean:
	rm -f $(TARGS) btree.asm