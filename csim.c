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
	unsigned int tag; // the tag of the line to know if it's valid
	int LRU; // least recently used (replacement policy)
} cacheLine;

typedef struct set {
	cacheLine* lines; // sets are made up of lines...
} cacheSet;

typedef struct cache {
	cacheSet* sets; // and caches are made up of sets
} Cache;

cacheInfo processCache(Cache* cache, cacheInfo info, unsigned long long address, int verbose) {
	int evictIndex;
	int minLRU;
	unsigned long long tag = address >> (info.s + info.b);
	unsigned long long setNum = (address >> info.b) & (info.S - 1);
	cacheSet currSet = cache->sets[setNum];

	for(int i = 0; i < info.E; i++) {
		cacheLine currLine = currSet.lines[i];
		if(currLine.valid) { // if the line we're examining is valid
			if(currLine.tag == tag) {
				info.numHits++;
				currLine.LRU++;
				if(verbose) { printf("hit "); }
				return info;
			}
		}
	}

	if(verbose) { printf("miss "); }
	info.numMisses++; // if we've made it to this point, we know there wasn't a hit and we missed

	for(int j = 0; j < info.E; j++) {
		cacheLine currLine = currSet.lines[j];
		if(j != 0) {
			if(minLRU > currLine.LRU) {
				minLRU = currLine.LRU;
				evictIndex = j;
			}
		}
		else {
			minLRU = currLine.LRU; // set the minimum LRU to the first line we look at
			evictIndex = j;
		}
	}

	if(cache->sets[setNum].lines[evictIndex].valid) {
		info.numEvicts++;
		if(verbose) { printf("eviction "); }
	}
	return info;

	cache->sets[setNum].lines[evictIndex].valid = 1;
	cache->sets[setNum].lines[evictIndex].tag = tag;
	cache->sets[setNum].lines[evictIndex].LRU = 0;

}

cacheInfo processFile(Cache* cache, cacheInfo info, int verbose, char* file) {
	char s[20];
	FILE* fp = fopen(file, "r"); // open the file to read, fp points to the beginning
	if(fp == NULL) {
		puts("File not found.");
		return info;
	}

	while(fgets(s, 20, fp) != NULL) {
		char c = s[1];
		unsigned long long address = 0;
		unsigned int len = 0;
		if(c == 'M' || c == 'L' || c == 'S') {
			fscanf(fp, " %c %llx,%u", &c, &address, &len);

			if(verbose != 0) { printf("%c %llx,%u\n", c, address, len); }
			if(c == 'M') { // if we have a miss, we need to process the info twice to move info back into the cache
				info = processCache(cache, info, address, verbose);
				info = processCache(cache, info, address, verbose);
			}
			else { // otherwise, all we have to do is process cache once
				info = processCache(cache, info, address, verbose);
			}

		}
	}

	return info;
}

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

/*
 * Allocate space for each of the lines and sets in the cache
 */
Cache* newCache(cacheInfo info) {
	Cache* cache = (Cache*) malloc(sizeof(Cache*));
	cache->sets = (cacheSet*) calloc(info.S, sizeof(cacheSet));

	//for each set, go through and initialize each line with validities of 0 and allocate space for it
	for(int i = 0; i < info.S; i++) {
		cacheLine* lines = (cache->sets[i].lines);
		lines = (cacheLine*) calloc(info.E, sizeof(cacheLine)); // allocate space for the line

		for(int j = 0; j < info.E; j++) {
			lines[j].valid = 0;
			lines[j].tag = 0;
			lines[j].LRU = 0;

		}
	}

	return cache;
}

int main(int argc, char* argv[]) {
	cacheInfo info;
	Cache* cache;
	char* file;
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

	info.S = 1 << info.s;
	info.B = 1 << info.b;
	info.numEvicts = 0;
	info.numHits = 0;
	info.numMisses = 0;
	cache = newCache(info);
	info = processFile(cache, info, verbose, file);


	printSummary(info.numEvicts, info.numHits, info.numMisses);
	return 0;
}
