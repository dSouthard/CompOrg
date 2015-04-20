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

  // track number of hits/misses/references; need to be unsigned to handle possible sizes
  unsigned long long hits, misses, writeRefs, readRefs, insRefs;

  // track kick-outs and dirty kick-outs
  unsigned long long dirtyKickouts, kickouts, transfers;

  // track cycles for activities
  unsigned long long instructionTime, readTime, writeTime;
  
  // how many cycles each hit/miss should add
  int hitTime, missTime;

  struct Block **blockArray;	// Array of pointers to blocks, implements LRU policy
};

#endif
