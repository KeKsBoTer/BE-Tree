all: bench_client

bench_client: bench_client.o ../bin/bptree.o 
	gcc -g  -I ../src -o bench_client bench_client.o ../bin/bptree.o -lpthread -lm -fsanitize=address

bench_client.o: bench_client.c
	gcc -g -I ../src -c bench_client.c -fsanitize=address

clean:
	-rm *.o bench_client