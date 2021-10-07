CC = gcc 
CFLAGS =  -Wall -m64 -mavx2
LDFLAGS = -lpthread -lm
TARGS = main 

ifeq ($(mode), debug)
CFLAGS += -g
else
CFLAGS += -O3
endif

all: $(TARGS)

main:  main.c
	$(CC) $(CFLAGS) $< -o main  $(LDFLAGS)

clean:
	rm -f *.o main *.asm 

clean_trees:
	rm -f trees/*.dot