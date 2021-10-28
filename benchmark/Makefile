all: bench_client

bench_client: bench_client.o
	gcc -g  -I ../src -o bench_client bench_client.o ../bin/*.o -lpthread -lpoet -lhb-acc-pow-shared -lhb-energy-msr -lm

bench_client.o: bench_client.c
	gcc -g -I ../src -c bench_client.c

clean:
	-rm *.o bench_client