#ifndef CACHE_H
#define CACHE_H

// Each block in a cache
struct Block {
  int valid;	// track if block is valid
  int dirty;	// track if block is dirty
  unsigned long long tag;	// block's tag

  // next block in LRU chain: if this is null, this is the LRU
  struct Block *nextBlock;	
};

// Each cache
struct Cache {
  int cacheSize, blockSize, associativity;	// provided from config file

  // how many rows in the cache
  int lengthOfWay;
  
  // size of index field & byte offset field
  int indexFieldSize, byteOffsetSize;	

  // track number of hits/misses/references
  long long hits, misses, writeRefs, readRefs;

  // track kick-outs and dirty kick-outs
  long long dirtyKickout, kickout;

  // track cycles for activities
  long long instructionTime, readTime, writeTime;

  // how many cycles each hit/miss should add
  int hitTime, missTime;

  // track the financial cost of the cache
  int cost;

  struct Block **blockArray;	// Array of pointers to blocks, implements LRU policy
};


// Function declarations
// Initialization for caches
struct Cache* initialize(int cachesize, int blocksize, int ways, int hittime, int misstime);

// Read Trace
unsigned long long readTrace(struct Cache* cache,unsigned long long  t, unsigned long long index, char op);

// Print Cache Status
void printCache(struct Cache *cache);

// Print Full Report
void printReport(struct Cache *icache, struct Cache* dcache, struct Cache* l2cache, int mready, int mchunks, int mchunkt, unsigned long long refnum, unsigned long long misallignment, int memcost);

// Calculate financial cost of the system
int cacheCost(int level, int size, int associativity);

#endif
