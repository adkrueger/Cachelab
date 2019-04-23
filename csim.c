/*
 * Authors:
 * Aaron Krueger (adkrueger)
 * Theo Campbell (tjcampbell)
 */
#include "cachelab.h"
#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <math.h>


typedef struct info {
	int numEvicts; // the number of cache evictions
	int numHits; // the number of cache hits
	int numMisses; // the number of cache misses
	int E; // the lines in the set
	int S; // the number of sets
	int s; // 2^s sets
	int B; // the bytes in the cache "payload"
	int b; // 2^e bytes per block
} cacheInfo;

typedef struct line {
	int valid; // the valid bit
	int tag; // the tag of the line to know if it's valid
	int lastUsed; // replacement policy: least recently used
} cacheLine;

typedef struct set {
	cacheLine* lines; // sets are made up of lines...
} cacheSet;

typedef struct cache {
	cacheSet* sets; // and caches are made up of sets
} Cache;

void printUsage() {
	puts("./csim-ref [-hv] -s <s> -E <E> -b <b> -t <tracefile>");
	puts("Where...");
	puts("\t• -h: Optional help flag that prints usage info\n"
			"\t• -v: Optional verbose flag that displays trace info\n"
			"\t• -s <s>: Number of set index bits (the number of sets is 2^s)\n"
			"\t• -E <E>: Associativity (number of lines per set)\n"
			"\t• -b <b>: Number of block bits (the block size is 2^b)\n"
			"\t• -t <tracefile>: Name of the valgrind trace to replay");
}

Cache newCache(cacheInfo info) {
	Cache cache;
	cache.sets = (cacheSet*) malloc(sizeof(Cache) * info.S);

	//for each set, go through and initialize each line with values of 0
	for(int i = 0; i < info.S; i++) {
		cacheLine* lines = cache.sets[i];
		lines = (cacheLine*) malloc(sizeof(cacheLine) * info.E); // allocate space for the line

		for(int j = 0; j < info.E; j++) {
			lines[j].valid = 0;
			cache.sets[i] = lines[j];
		}
	}

	return cache;
}

int main(int argc, char* argv[]) {
	cacheInfo info = (cacheInfo) malloc(32);
	Cache cache;
	FILE* file;
	int opt;
	int verbose;
	while((opt = getopt(argc, argv, "hvs:E:b:t:")) != -1) {
		switch(opt) {
		case 'h':
			printUsage();
			break;
		case 'v':
			verbose = 1;
			break;
		case 's':
			info.s = atoi(optarg);
			break;
		case 'E':
			info.E = atoi(optarg);
			break;
		case 'b':
			info.b = atoi(optarg);
			break;
		case 't':
			file = optarg;
			break;
		default:
			puts("Found incorrect value.\n");
			printUsage();
			break;
		}
	}

	info.S = pow(2, info.s);
	info.B = pow(2, info.b);
	info.numEvicts = 0;
	info.numHits = 0;
	info.numMisses = 0;
	cache = newCache(info);
	printSummary(info.numEvicts, info.numHits, info.numMisses);
	return 0;
}
