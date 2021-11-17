CC = gcc 
CFLAGS = -g -Wall
LDFLAGS = -lpthread -lm -lpoet -lhb-acc-pow-shared -lhb-energy-msr -lm

INCLUDE = -I ../include -I ./include 

all: bin/bench_client

bin/db.o: src/db.c
	$(CC) $(CFLAGS) $(INCLUDE) -c src/db.c -o bin/db.o

bin/bench_client.o: src/bench_client.c
	$(CC) $(CFLAGS) $(INCLUDE) -c src/bench_client.c -o bin/bench_client.o

bin/bench_client: bin/db.o bin/bench_client.o ../bin/bptree.o 
	$(CC) $(CFLAGS) $(INCLUDE) bin/db.o bin/bench_client.o ../bin/bptree.o -o bin/bench_client $(LDFLAGS)
clean:
	rm -rf bin/*