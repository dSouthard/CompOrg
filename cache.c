#include "cache.h"


#include<stdlib.h>
#include<math.h>
#include<stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>

/*********************** Initialization ***********************/
struct Cache * initialize(int newCacheSize, int newBlockSize, int newAssociativity, int newMissTime, int newHitTime) {
    struct Cache *cache;

    // Create in memory  
    cache = (struct Cache*) malloc(sizeof (struct Cache));

    // Initialize cache variable fields based on configuration files
    cache->cacheSize = newCacheSize;
    cache->blockSize = newBlockSize;
    cache->associativity = newAssociativity;
    cache->lengthOfWay = (newCacheSize / newBlockSize) / newAssociativity;
    cache->indexFieldSize = log(cache->lengthOfWay) / log(2); // log(base2)(lengthOfWay)
    cache->byteOffsetSize = log(newBlockSize) / log(2); // log(base2)(newBlockSize)
    cache->missTime = newMissTime;
    cache->hitTime = newHitTime;

    // Initialize tracking variables to 0
    cache->hits = 0;
    cache->misses = 0;
    cache->writeRefs = 0;
    cache->readRefs = 0;
    cache->insRefs = 0;
    cache->instructionTime = 0;
    cache->readTime = 0;
    cache->writeTime = 0;
    cache->dirtyKickouts = 0;
    cache->kickouts = 0;

    // Initialize array of blocks
    cache->blockArray = (struct Block **) malloc((cache->lengthOfWay * sizeof (struct Block *)));

    int i = 0, j = 0; // used for iteration
    for (i = 0; i < cache->lengthOfWay; i++) {
        // First column in blockArray will be a dummy pointer => add an extra "way"(column)
        // Go through each row and allocate space for necessary amount of blocks
        cache->blockArray[i] = (struct Block *) malloc((newAssociativity + 1) * sizeof (struct Block));

        // Go through and initialize blocks
        for (j = 0; j < newAssociativity; j++) {
            // Set valid, dirty, and tag fields
            cache->blockArray[i][j].valid = 0;
            cache->blockArray[i][j].dirty = 0;
            cache->blockArray[i][j].tag = 0;

            // Point to next block in LRU chain
            cache->blockArray[i][j].nextBlock = &(cache->blockArray[i][j + 1]);
        }

        // Point last block to null
        cache->blockArray[i][newAssociativity].nextBlock = NULL;
    }

    // Done
    return cache;
}
//
///********************* Read Trace Function ***************************/

unsigned long long moveBlock(struct Cache* cache, unsigned long long targetTag, unsigned long long targetIndex) {
    struct Block *tempBlock; // Used for LRU policy implementation

    // Point to the first block in the index
    tempBlock = cache->blockArray[targetIndex][0].nextBlock;

    while ((tempBlock->nextBlock != NULL) || (tempBlock->valid == 0)) { // Find the LRU or first invalid block
        tempBlock = tempBlock->nextBlock;
    }

    if ((!tempBlock->valid) || (!tempBlock->dirty)) {
        // Block is invalid or clean, feel free to write over it
        tempBlock->tag = targetTag;
        cache->kickouts++;
        cache->writeRefs++;
        cache->writeTime += cache->hitTime;

        // Update LRU chain
        // Move block to front of LRU chain
        struct Block * firstBlock = cache->blockArray[targetIndex][0].nextBlock; // Keep track of old chain start
        cache->blockArray[targetIndex][0].nextBlock = tempBlock; // Put block at start of chain
        tempBlock = firstBlock;
        while (tempBlock->nextBlock != cache->blockArray[targetIndex][0].nextBlock) {
            // Run through chain until you find the block that used to be before tempBlock
            tempBlock = tempBlock->nextBlock;
        }
        struct Block * temp2 = cache->blockArray[targetIndex][0].nextBlock;
        tempBlock->nextBlock = temp2->nextBlock;
        temp2->nextBlock = firstBlock;

        return 0;
    }

    unsigned long long returnedTag = tempBlock->tag; // track written over dirty tag
    cache->dirtyKickouts++;
    cache->writeRefs++;
    cache->writeTime += cache->hitTime;

    tempBlock->tag = targetTag; // Put in new targetTag into block

    // Update LRU chain
    // Move block to front of LRU chain
    struct Block * firstBlock = cache->blockArray[targetIndex][0].nextBlock; // Keep track of old chain start
    cache->blockArray[targetIndex][0].nextBlock = tempBlock; // Put block at start of chain
    tempBlock = firstBlock;
    while (tempBlock->nextBlock != cache->blockArray[targetIndex][0].nextBlock) {
        // Run through chain until you find the block that used to be before tempBlock
        tempBlock = tempBlock->nextBlock;
    }
    struct Block * temp2 = cache->blockArray[targetIndex][0].nextBlock;
    tempBlock->nextBlock = temp2->nextBlock;
    temp2->nextBlock = firstBlock;

    return returnedTag; // signal that there was a dirty kick out
}

int scanCache(struct Cache* cache, unsigned long long targetTag, unsigned long long targetIndex, char op) {
    struct Block * tempBlock; // Used for LRU policy implementation

    // Point to the first block in the index
    tempBlock = cache->blockArray[targetIndex][0].nextBlock;

    while (tempBlock != NULL) {
        if (tempBlock->valid) { // Only check blocks if they are valid
            if (tempBlock->tag == targetTag) {
                // Found the tag! Increment hits
                cache->hits++;

                if (op == 'W') { // valid targetTag found in cache, write operation => mark block as dirty
                    tempBlock->dirty = 1;
                    cache->writeTime += cache->hitTime; // increase write time by hit penalty
                    cache->writeRefs++;
                } else if (op == 'R') { // Instructions/Reads, instructions are the same as reads in the L1 I cache
                    cache->readTime += cache->hitTime; // increase write time by hit penalty
                    cache->readRefs++;
                } else if (op == 'I') {
                    cache->instructionTime += cache->hitTime; // increase write time by hit penalty
                    cache->insRefs++;
                }

                // Move block to front of LRU chain
                struct Block * firstBlock = cache->blockArray[targetIndex][0].nextBlock; // Keep track of old chain start
                cache->blockArray[targetIndex][0].nextBlock = tempBlock; // Put block at start of chain
                tempBlock = firstBlock;
                while (tempBlock->nextBlock != cache->blockArray[targetIndex][0].nextBlock) {
                    // Run through chain until you find the block that used to be before tempBlock
                    tempBlock = tempBlock->nextBlock;
                }
                struct Block * temp2 = cache->blockArray[targetIndex][0].nextBlock;
                tempBlock->nextBlock = temp2->nextBlock;
                temp2->nextBlock = firstBlock;

                return 1;
            }
        }

        else
            tempBlock = tempBlock->nextBlock;
    }
    // Target tag not found in cache
    cache->misses++;

    if (op == 'W') {
        cache->writeTime += cache->missTime; // increase write time by hit penalty
        cache->writeRefs++;
    } else { // Instructions/Reads, instructions are the same as reads in the L1 I cache
        cache->readTime += cache->missTime; // increase write time by hit penalty
        cache->readRefs++;
    }

    return 0;
}