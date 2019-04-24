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
	int maxLRU;
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

int findEvictIndex(cacheSet currSet, cacheInfo info) {
	int evictIndex;
	int minLRU;
	for(int i = 0; i < info.E; i++) {
		cacheLine currLine = currSet.lines[i];
		if(i != 0) {
			if(minLRU > currLine.LRU) { // if we've found a line that's less than the minimum
				minLRU = currLine.LRU;
				evictIndex = i;
			}
		}
		else { // set the minimum LRU to the first index we look at
			minLRU = currLine.LRU;
			evictIndex = i;
		}
	}

	return evictIndex;
}

/*
 * Process the cache and adjust the number of hits, misses evictions
 */
cacheInfo processCache(Cache* cache, cacheInfo info, unsigned long long address, int verbose) {
	unsigned long long tag = address >> (info.s + info.b); // find the tag (which is shifted over by s and b to be comparable to our tag)
	unsigned long long setNum = (address >> info.b) & (info.S - 1); // find the appropriate number of the set
	cacheSet currSet = cache->sets[setNum];
	int evictIndex;

	for(int i = 0; i < info.E; i++) {
		cacheLine currLine = currSet.lines[i];
		if(currLine.valid && currLine.tag == tag) { // if the line we're examining is valid and the tag matches, then hit
			if(verbose) { printf("hit "); }
			info.numHits++;
			currLine.LRU++; // increment LRU because we've used this value again
			if(currLine.LRU > info.maxLRU) { info.maxLRU = currLine.LRU; }
			return info;
		}
	}

	if(verbose) { printf("miss "); }
	info.numMisses++; // if we've made it to this point, we know there wasn't a hit and we missed

	// find the lines[] index of the line we are going to evict
	evictIndex = findEvictIndex(currSet, info);

	// if the line we are going to evict is valid, then increase the number of evictions
	if(currSet.lines[evictIndex].valid) {
		if(verbose) { printf("eviction "); }
		info.numEvicts++;
	}

	// set the info of the line to be properly evicted
	currSet.lines[evictIndex].valid = 1; // set validity to 1 (as we now know for sure that this line is valid)
	currSet.lines[evictIndex].tag = tag; // set the tag
	currSet.lines[evictIndex].LRU = info.maxLRU; // need to set to the maxLRU because this is now the most recently used line

	return info;
}

/*
 * Process the file's input and run processCache according to the trace file
 */
cacheInfo processFile(Cache* cache, cacheInfo info, int verbose, char* file) {
	char s[20];
	char c; // get the first character we need (either L, M, or S)

	FILE* fp = fopen(file, "r"); // open the file to read, fp points to the beginning
	if(fp == NULL) { // if there's no file then don't continue
		puts("File not found.");
		return info;
	}

	while(fgets(s, 20, fp) != NULL) {
		unsigned long long address = 0; // the address in the trace file given by valgrind
		unsigned int len = 0; // the length given by valgrind
		sscanf(s, " %c %llx,%u", &c, &address, &len); // initialize  each variable to what we have in the trace file

		if(verbose && c != 'I') { printf("%c %llx,%u ", c, address, len); }
		if(c == 'M') { // if we have a miss, we need to process the info twice to move info back into the cache
			info = processCache(cache, info, address, verbose);
			info = processCache(cache, info, address, verbose);
		}
		else if(c == 'L' || c == 'S') {  // otherwise, all we have to do is process cache once
			info = processCache(cache, info, address, verbose);
		}
		if(verbose && c != 'I') { printf("\n"); }
	}
	fclose(fp);
	return info;
}

/*
 * print out the program usage
 */
void printUsage() {
	puts("USAGE:");
	puts("./csim [-hv] -s <s> -E <E> -b <b> -t <tracefile>");
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
	Cache* cache = (Cache*) malloc(sizeof(Cache));
	cache->sets = (cacheSet*) malloc(info.S * sizeof(cacheSet));

	//for each set, go through and initialize each line with values of 0 and allocate space for it
	for(int i = 0; i < info.S; i++) {
		cacheLine* lines = (cache->sets[i].lines);
		lines = (cacheLine*) malloc(info.E * sizeof(cacheLine)); // allocate space for the line

		for(int j = 0; j < info.E; j++) {
			lines[j].valid = 0;
			lines[j].tag = 0;
			lines[j].LRU = 0;
		}
		cache->sets[i].lines = lines;
	}

	return cache;
}

int main(int argc, char* argv[]) {
	cacheInfo info;
	Cache* cache;
	char* file;
	int opt;
	char verbose;

	// use getopt to read optional flags and their values
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

	info.S = 1 << info.s;  // find out the proper S value
	info.B = 1 << info.b; // find out the proper B value
	info.numEvicts = 0; //
	info.numHits = 0;   // reset each counter to 0
	info.numMisses = 0; //
	info.maxLRU = 0;

	cache = newCache(info);
	info = processFile(cache, info, verbose, file);


	printSummary(info.numHits, info.numMisses, info.numEvicts);
	return 0;
}
