# g++ >= 4.9 or clang++ >= 3.4 is required to build this.
# clang++ 3.4 and 3.5 cannot build the compare target with debug information.
# Either remove "-g" from the flags or use clang++ >= 3.6 (or g++).
CC = gcc

SANITIZER ?= address

COMMONFLAGS = -std=c++1y -Wall -Wextra -Werror
CFLAGS = ${COMMONFLAGS} -Ofast -g -DNDEBUG
DEBUGFLAGS = ${COMMONFLAGS} -O0 -ggdb3
LDFLAGS = -lpapi -lboost_serialization -lstdc++ -lprimesieve
MALLOC_LDFLAGS = -ldl

all: bench_hash bench_pq

everything: bench_hash bench_pq bench_hash_malloc compare bench_pq_malloc debug_hash debug_pq sanitize_hash sanitize_pq

clean:
	rm -f *.o bench_hash bench_hash_malloc bench_pq bench_pq_malloc \
		debug_hash debug_pq sanitize_hash sanitize_pq

malloc_count.o: malloc_count/malloc_count.c  malloc_count/malloc_count.h
	$(CC) -O2 -Wall -Werror -g -c -o $@ $<

bench_hash: bench_hash.cpp common/*.h hashtable/*.h
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

bench_hash_malloc: bench_hash.cpp malloc_count.o common/*.h hashtable/*.h
	$(CC) $(CFLAGS) -DMALLOC_INSTR -o $@ $< malloc_count.o $(LDFLAGS) $(MALLOC_LDFLAGS)

bench_pq: bench_pq.cpp common/*.h pq/*.h
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

bench_pq_malloc: bench_pq.cpp malloc_count.o common/*.h pq/*.h
	$(CC) $(CFLAGS) -DMALLOC_INSTR -o $@ $< malloc_count.o $(LDFLAGS) $(MALLOC_LDFLAGS)

debug_hash: bench_hash.cpp common/*.h hashtable/*.h
	$(CC) $(DEBUGFLAGS) -o $@ $< $(LDFLAGS)

debug_hash_malloc: bench_hash.cpp malloc_count.o common/*.h hashtable/*.h
	$(CC) $(DEBUGFLAGS) -DMALLOC_INSTR -o $@ $< malloc_count.o $(LDFLAGS) $(MALLOC_LDFLAGS)

debug_pq: bench_pq.cpp common/*.h pq/*.h
	$(CC) $(DEBUGFLAGS) -o $@ $< $(LDFLAGS)

debug_pq_malloc: bench_pq.cpp malloc_count.o common/*.h pq/*.h
	$(CC) $(DEBUGFLAGS) -DMALLOC_INSTR -o $@ $< malloc_count.o $(LDFLAGS) $(MALLOC_LDFLAGS)

sanitize_hash: bench_hash.cpp common/*.h hashtable/*.h
	$(CC) $(CFLAGS) -fsanitize=${SANITIZER} -o $@ $< $(LDFLAGS)
	./$@

sanitize_pq: bench_pq.cpp common/*.h pq/*.h
	$(CC) $(CFLAGS) -fsanitize=${SANITIZER} -o $@ $< $(LDFLAGS)
	./$@

compare: compare.cpp common/*.h
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

run_hash: bench_hash
	./bench_hash

run_hash_malloc: bench_hash_malloc
	./bench_hash_malloc

run_pq: bench_pq
	./bench_pq

run_pq_malloc: bench_pq_malloc
	./bench_pq_malloc

