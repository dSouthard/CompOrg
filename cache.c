#include "cache.h"


#include<stdlib.h>
#include<math.h>
#include<stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define DEBUG
#define POINTERDEBUG

void updateLRU(struct Cache* cache, struct Block * tempBlock, unsigned long long targetIndex);

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
    cache->transfers = 0;
    cache->kickouts = 0;

    // Initialize array of blocks
    cache->blockArray = (struct Block **) malloc((cache->lengthOfWay * sizeof (struct Block *)));

    int i = 0, j = 0; // used for iteration
    for (i = 0; i < cache->lengthOfWay; i++) {
        // First column in blockArray will be a dummy pointer => add an extra "way"(column)
        // Go through each row and allocate space for necessary amount of blocks
        cache->blockArray[i] = (struct Block *) malloc((newAssociativity + 1) * sizeof (struct Block));

        // Go through and initialize blocks (columns)
        for (j = 0; j < newAssociativity; j++) {

            // Set valid, dirty, and tag fields
            cache->blockArray[i][j].valid = 0;
            cache->blockArray[i][j].dirty = 0;
            cache->blockArray[i][j].tag = 0;

            // Point to next block in LRU chain
            cache->blockArray[i][j].nextBlock = &(cache->blockArray[i][j + 1]);
#ifdef POINTERDEBUG
            printf("|| [%p] points to: %p || \t", &cache->blockArray[i][j], cache->blockArray[i][j].nextBlock);
#endif
        }

        // Point last block to null
        cache->blockArray[i][newAssociativity].nextBlock = NULL;
#ifdef POINTERDEBUG
        printf("|| [%p] points to: %p || \n", &cache->blockArray[i][newAssociativity], cache->blockArray[i][newAssociativity].nextBlock);
#endif
    }

    // Done
    return cache;
}
//
///********************* Read Trace Function ***************************/

unsigned long long moveBlock(struct Cache* cache, unsigned long long targetTag, unsigned long long targetIndex) {
#ifdef DEBUG
    printf("~~~> In the moveBlock function: ");
#endif
    struct Block *tempBlock; // Used for LRU policy implementation

    // Point to the first block in the index
    tempBlock = cache->blockArray[targetIndex][0].nextBlock;
#ifdef DEBUG
    printf("Moved tempBlock to beginning of LRU chain...");
#endif

    while ((tempBlock->nextBlock != NULL) && (tempBlock->valid == 1)) { // Find the LRU or first invalid block
        tempBlock = tempBlock->nextBlock;
    }

#ifdef DEBUG
    printf("tempBlock now pointing at LRU/1st invalid block...\n");
#endif
    if ((!tempBlock->valid) || (!tempBlock->dirty)) {
#ifdef DEBUG
        printf("~~~> Block was clean/invalid: good to overwrite.\n");
#endif
        // Block is invalid or clean, feel free to write over it
        tempBlock->tag = targetTag;
        tempBlock->valid = 1;
        cache->kickouts++;
        cache->writeRefs++;
        cache->writeTime += cache->hitTime;

        // Update LRU chain
        // Move block to front of LRU chain
        if (cache->blockArray[targetIndex][0].nextBlock != tempBlock) {
#ifdef DEBUG
            printf("~~~> tempBlock is not the head of the LRU chain, time to reorganize.\n");
#endif
            updateLRU(cache, tempBlock, targetIndex);
        } else {
#ifdef DEBUG
            printf("~~~> tempBlock is already the head of the LRU chain, all is good.\n");
#endif
        }

#ifdef DEBUG
        printf("~~~> Exiting the moveBlock function successfully.\n");
#endif
        return 0;
    }

#ifdef DEBUG
    printf("~~~> Block was dirty, need to return overwritten tag...");
#endif
    unsigned long long returnedTag = tempBlock->tag; // track written over dirty tag
    cache->dirtyKickouts++;
    cache->writeRefs++;
    cache->writeTime += cache->hitTime;

    tempBlock->tag = targetTag; // Put in new targetTag into block

#ifdef DEBUG
    printf("and update the LRU chain...\n");
#endif
    // Update LRU chain
    // Move block to front of LRU chain
    if (cache->blockArray[targetIndex][0].nextBlock != tempBlock) {
#ifdef DEBUG
        printf("~~~> tempBlock is not the head of the LRU chain, time to reorganize.\n");
#endif
        updateLRU(cache, tempBlock, targetIndex);
    } else {
#ifdef DEBUG
        printf("~~~> tempBlock is already the head of the LRU chain, all is good.\n");
#endif
    }

#ifdef DEBUG
    printf("~~~> Sucessfully exiting the moveBlock function, returning dirty tag.\n");
#endif
    return returnedTag; // signal that there was a dirty kick out
    //    return 0;
}

int scanCache(struct Cache* cache, unsigned long long targetTag, unsigned long long targetIndex, char op) {
#ifdef DEBUG
    printf("~~~> In the scanCache function: ");
#endif
    struct Block * tempBlock; // Used for LRU policy implementation

    // Point to the first block in the index

    tempBlock = cache->blockArray[targetIndex][0].nextBlock;
#ifdef DEBUG
    printf("tempBlock is now pointing to start of LRU chain\n");
#endif

    while (tempBlock != NULL) {
#ifdef DEBUG
        printf("~~~> Moving through the chain...");
#endif
        if (tempBlock->valid) { // Only check blocks if they are valid
#ifdef DEBUG
            printf("Checking a valid block...");
#endif
            if (tempBlock->tag == targetTag) {
#ifdef DEBUG
                printf("It's a match!\n");
#endif
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
                if (cache->blockArray[targetIndex][0].nextBlock != tempBlock) {
#ifdef POINTERDEBUG
                    printf("~~~> tempBlock is not the head of the LRU chain, time to reorganize.\n");
#endif
                    updateLRU(cache, tempBlock, targetIndex);
                } else {
#ifdef POINTERDEBUG
                    printf("~~~> tempBlock is already the head of the LRU chain, all is good.\n");
#endif
                }

                return 1;
            }
        } else
#ifdef DEBUG
            printf("It's not a match, moving on to the next block.\n");
#endif
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

// Purpose: move tempBlock to the front of the LRU chain with index targetIndex

void updateLRU(struct Cache* cache, struct Block * tempBlock, unsigned long long targetIndex) {
#ifdef DEBUG
    printf("Starting updateLRU function...");
#endif
    // firstBlock points to the old start of the chain
    struct Block * firstBlock = cache->blockArray[targetIndex][0].nextBlock;
#ifdef DEBUG
    printf("firstBlock is now pointing to the front of the LRU chain...");
#endif

    cache->blockArray[targetIndex][0].nextBlock = tempBlock; // Put block at start of chain
#ifdef DEBUG
    printf("tempBlock's block is now the start of the LRU chain.\n");
#endif

    tempBlock = firstBlock;
#ifdef DEBUG
    printf("tempBlock is now pointing to the old front of the LRU chain...");
#endif
    while (tempBlock->nextBlock != cache->blockArray[targetIndex][0].nextBlock) {
        // Run through chain until you find the block that used to be before tempBlock
        tempBlock = tempBlock->nextBlock;
    }
#ifdef DEBUG
    printf("and now is pointing to the block that used to be before the new front of the LRU chain\n");
#endif

    struct Block * temp2 = cache->blockArray[targetIndex][0].nextBlock;
#ifdef DEBUG
    printf("temp2 is now pointing to the new start of the LRU chain...");
#endif
    tempBlock->nextBlock = temp2->nextBlock;
#ifdef DEBUG
    printf("and now the block before doesn't point to the new front, it points to the one that came after it.\n ");
#endif
    temp2->nextBlock = firstBlock;
#ifdef DEBUG
    printf("Now the new front points to the old front, and the LRU chain is restored.");
#endif
}

void printCacheStatus(struct Cache * cache) {
    printf("----------------------------------------------------\n");
    printf("---------------  Cache Status  ---------------------\n");
    printf("----------------------------------------------------\n");

    // Cache variables not already printed in printSetup function at beginning
    printf("Cache length: %d, index field size: %d, byte field size: %d\n",
            cache->lengthOfWay, cache->indexFieldSize, cache->byteOffsetSize);

    printf("Number of hits: %llu, misses: %llu, kick-outs: %llu, dirty kick-outs: %llu\n",
            cache->hits, cache->misses, cache->kickouts, cache->dirtyKickouts);

    printf("Reference Counts: writes = %llu, reads = %llu, instructions = %llu\n",
            cache->writeRefs, cache->readRefs, cache->insRefs);

    printf("Times: writes = %llu, reads = %llu, instructions = %llu\n",
            cache->writeTime, cache->readTime, cache->instructionTime);

    // Go through each row of the cache
    printf("------ Current Row Status ------\n");
    int i = 0, j = 0;
    for (i = 0; i < cache->lengthOfWay; i++) {
        printf("Index %llx: ", (unsigned long long) i);
        for (j = 1; j < (cache->associativity + 1); j++) {
            // Skip the first column since it's being used as a dummy pointer
            if (cache->blockArray[i][j].tag) // Block has a tag in it
                printf("| Tag: %llx, V: %d, D: %d  |",
                    cache->blockArray[i][j].tag, cache->blockArray[i][j].valid, cache->blockArray[i][j].dirty);
            else // Block doesn't have a tag in it
                printf("| Tag: -, V: %d, D: %d  |",
                    cache->blockArray[i][j].valid, cache->blockArray[i][j].dirty);
        }
        printf("\n\n");
    }

#ifdef POINTERDEBUG
    printf("~~~> ---------------- Pointer Contents ----------------\n\n");
    int row = 0, col = 0;
    for (row = 0; row < cache->lengthOfWay; row++) {
        printf("~~~> Index: %llx", (unsigned long long)row);
        for (col = 0; col < cache->associativity+1;col++){
            struct Block* thisBlock =&cache->blockArray[row][col];
            printf("||This block:%p, points to %p||\t", thisBlock, thisBlock->nextBlock);
        }
        printf("\n");
    }
#endif
    printf("End of Cache Status\n\n");
}