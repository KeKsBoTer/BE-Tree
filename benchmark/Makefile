CC = gcc 
CFLAGS = -g -Wall
LDFLAGS = -lpthread -lm -lpoet -lhb-acc-pow-shared -lhb-energy-msr -lm

INCLUDE = -I ../include -I ./include 

all: bin/bench_store_poet bin/bench_store

bin/queries.o: src/queries.c
	$(CC) $(CFLAGS) $(INCLUDE) -c src/queries.c -o bin/queries.o

bin/db.o: src/db.c
	$(CC) $(CFLAGS) $(INCLUDE) -c src/db.c -o bin/db.o

bin/bench_store_poet: src/bench_store_poet.c bin/db.o bin/queries.o ../bin/bptree.o 
	$(CC) $(CFLAGS) $(INCLUDE) bin/db.o src/bench_store_poet.c bin/queries.o ../bin/bptree.o -o bin/bench_store_poet $(LDFLAGS)
	
bin/bench_store: src/bench_store.c bin/db.o bin/queries.o ../bin/bptree.o 
	$(CC) $(CFLAGS) $(INCLUDE) bin/db.o src/bench_store.c bin/queries.o ../bin/bptree.o -o bin/bench_store $(LDFLAGS)
	
clean:
	rm -rf bin/*