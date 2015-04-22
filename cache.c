#include "cache.h"


#include<stdlib.h>
#include<math.h>
#include<stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>

//#define CACHE_DEBUG
//#define CACHE_POINTER_DEBUG
//#define CACHE_INITIALIZE_DEBUG

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
    cache->transfers = 0;
    cache->kickouts = 0;
    cache->dirtyKickouts = 0;
    cache->flushKickouts = 0;

    // Initialize array of blocks
    cache->blockArray = (struct Block **) malloc(sizeof (struct Block *) * (cache->lengthOfWay));

    int i = 0, j = 0; // used for iteration
    for (i = 0; i < cache->lengthOfWay; i++) {
        // First column in blockArray will be a dummy pointer => add an extra "way"(column)
        // Go through each row and allocate space for necessary amount of blocks
        cache->blockArray[i] = (struct Block *) malloc(sizeof (struct Block) * (newAssociativity + 1));

        // Go through and initialize blocks (columns)
        for (j = 0; j < newAssociativity; j++) {

            // Set valid, dirty, and tag fields
            cache->blockArray[i][j].valid = 0;
            cache->blockArray[i][j].dirty = 0;
            cache->blockArray[i][j].tag = 0;

            // Point to next block in LRU chain
            cache->blockArray[i][j].nextBlock = &(cache->blockArray[i][j + 1]);
#ifdef CACHE_INITIALIZE_DEBUG
            printf("|| [%p] points to: %p || \t", &cache->blockArray[i][j], cache->blockArray[i][j].nextBlock);
#endif
        }

        // Initialize last block to null
        cache->blockArray[i][newAssociativity].nextBlock = NULL; // Point to null
        cache->blockArray[i][newAssociativity].valid = 0;
        cache->blockArray[i][newAssociativity].dirty = 0;
        cache->blockArray[i][newAssociativity].tag = 0;
#ifdef CACHE_INITIALIZE_DEBUG
        printf("|| [%p] points to: %p || \n", &cache->blockArray[i][newAssociativity], cache->blockArray[i][newAssociativity].nextBlock);
#endif
    }

    // Done
    return cache;
}
//
///********************* Read Trace Function ***************************/

unsigned long long moveBlock(struct Cache* cache, unsigned long long targetTag, unsigned long long targetIndex) {
#ifdef CACHE_DEBUG
    printf("\n~~~> <~~~\n");
    printf("~~~> In the moveBlock function: \n");
#endif
    struct Block *tempBlock; // Used for LRU policy implementation

    // Point to the first block in the index
#ifdef CACHE_POINTER_DEBUG
    printf(" >>>>> The cache index is pointing to: %p\n", cache->blockArray[targetIndex][0].nextBlock);
#endif

    if (cache->blockArray[targetIndex][0].nextBlock != NULL)
        tempBlock = cache->blockArray[targetIndex][0].nextBlock;
    else {
        printf("!!!! Cache dummy pointer is pointing to a NULL block!!\n");
        printf("~~~> <~~~\n\n");
        return 0;
    }

#ifdef CACHE_POINTER_DEBUG
    printf(" >>>>> tempBlock is now pointing to: %p\n", tempBlock);
    printf(" >>>>> Next in line is: %p\n", tempBlock->nextBlock);
#endif

#ifdef CACHE_DEBUG
    printf("~~~> Moved tempBlock to beginning of LRU chain...\n");
#endif

    while ((tempBlock->nextBlock != NULL) && (tempBlock->valid == 1)) { // Find the LRU or first invalid block
        tempBlock = tempBlock->nextBlock;
#ifdef CACHE_POINTER_DEBUG
        printf(" >>>>> tempBlock is now pointing to: %p\n", tempBlock);
        printf(" >>>>> Next in line is: %p\n", tempBlock->nextBlock);
#endif
    }

#ifdef CACHE_DEBUG
    printf("~~~> tempBlock now pointing at LRU/1st invalid block...\n");
#endif
    if ((!tempBlock->valid) || (!tempBlock->dirty)) {
#ifdef CACHE_DEBUG
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
#ifdef CACHE_DEBUG
            printf("~~~> tempBlock is not the head of the LRU chain, time to reorganize.\n");
#endif
            updateLRU(cache, tempBlock, targetIndex);
        } else {
#ifdef CACHE_DEBUG
            printf("~~~> tempBlock is already the head of the LRU chain, all is good.\n");
#endif
        }

#ifdef CACHE_DEBUG
        printf("~~~> Exiting the moveBlock function successfully.\n");
#endif
#ifdef CACHE_POINTER_DEBUG
        printf(" >>>>> The cache index is pointing to: %p\n", cache->blockArray[targetIndex][0].nextBlock);
        printf("~~~> <~~~\n\n");
#endif
        return 0;
    }

#ifdef CACHE_DEBUG
    printf("~~~> Block was dirty, need to return overwritten tag...");
#endif
    unsigned long long returnedTag = tempBlock->tag; // track written over dirty tag
    cache->dirtyKickouts++;
    cache->writeRefs++;
    cache->writeTime += cache->hitTime;

    tempBlock->tag = targetTag; // Put in new targetTag into block

#ifdef CACHE_DEBUG
    printf("and update the LRU chain...\n");
#endif
    // Update LRU chain
    // Move block to front of LRU chain
    if (cache->blockArray[targetIndex][0].nextBlock != tempBlock) {
#ifdef CACHE_DEBUG
        printf("~~~> tempBlock is not the head of the LRU chain, time to reorganize.\n");
#endif
        updateLRU(cache, tempBlock, targetIndex);
    } else {
#ifdef CACHE_DEBUG
        printf("~~~> tempBlock is already the head of the LRU chain, all is good.\n");
#endif
    }

#ifdef CACHE_DEBUG
    printf("~~~> Successfully exiting the moveBlock function, returning dirty tag.\n");
#endif

#ifdef CACHE_POINTER_DEBUG
    printf("\n >>>>> The cache index is pointing to: %p\n", cache->blockArray[targetIndex][0].nextBlock);
    printf("~~~> <~~~\n\n");
#endif
    return returnedTag; // signal that there was a dirty kick out
}

int scanCache(struct Cache* cache, unsigned long long targetTag, unsigned long long targetIndex, char op) {
#ifdef CACHE_DEBUG
    printf("\n~~~> <~~~\n");
    printf("~~~> In the scanCache function: ");
#endif
    struct Block * tempBlock; // Used for LRU policy implementation

    // Point to the first block in the index
#ifdef CACHE_POINTER_DEBUG
    printf("\n >>>>> The cache index is pointing to: %p\n", cache->blockArray[targetIndex][0].nextBlock);
#endif

    tempBlock = cache->blockArray[targetIndex][0].nextBlock;

#ifdef CACHE_DEBUG
    printf("~~~> tempBlock is now pointing to start of LRU chain\n");
#endif

    int wayNumber = 1;
    while (tempBlock != NULL) {
#ifdef CACHE_DEBUG
        printf("~~~> Moving through the chain...Looking at way #%d", wayNumber);
#endif
#ifdef CACHE_POINTER_DEBUG
        printf("\n >>>>> tempBlock is now pointing to: %p\n", tempBlock);
        printf(" >>>>> Next in line is: %p\n", tempBlock->nextBlock);
#endif
        if (tempBlock->valid) { // Only check blocks if they are valid
#ifdef CACHE_DEBUG
            printf("Checking a valid block...");
#endif
            if (tempBlock->tag == targetTag) {
#ifdef CACHE_DEBUG
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
#ifdef CACHE_POINTER_DEBUG
                    printf("~~~> tempBlock is not the head of the LRU chain, time to reorganize.\n");
#endif
                    updateLRU(cache, tempBlock, targetIndex);
                } else {
#ifdef CACHE_POINTER_DEBUG
                    printf("~~~> tempBlock is already the head of the LRU chain, all is good.\n");
                    printf("~~~> <~~~\n\n");
#endif
                }

                return 1;
            } else { // Tag didn't match
#ifdef CACHE_DEBUG
                printf("~~~> Valid tag didn't match\n");
#endif
                tempBlock = tempBlock->nextBlock;
                wayNumber++;
            }
        } else {
            wayNumber++;
#ifdef CACHE_DEBUG
            printf("~~~> It's not valid\n");
#endif
            tempBlock = tempBlock->nextBlock;
        }
    }
    // Target tag not found in cache
    cache->misses++;

    if (op == 'W') {
        cache->writeTime += cache->missTime; // increase write time by hit penalty
        cache->writeRefs++;
    } else if (op == 'R') {
        cache->readTime += cache->missTime; // increase write time by hit penalty
        cache->readRefs++;
    } else if (op == 'I') {
        cache->instructionTime += cache->missTime; // increase write time by hit penalty
        cache->insRefs++;
    }

#ifdef CACHE_DEBUG
    printf("~~~> <~~~\n\n");
#endif

    return 0;
}

// Purpose: move tempBlock to the front of the LRU chain with index targetIndex

void updateLRU(struct Cache* cache, struct Block * tempBlock, unsigned long long targetIndex) {
#ifdef CACHE_DEBUG
    printf("\n~~~> <~~~\n");
    printf("Starting updateLRU function...");
#endif
    // firstBlock points to the old start of the chain
    struct Block * firstBlock = cache->blockArray[targetIndex][0].nextBlock;
#ifdef CACHE_DEBUG
    printf("firstBlock is now pointing to the front of the LRU chain...");
#endif

    if (tempBlock != NULL)
        cache->blockArray[targetIndex][0].nextBlock = tempBlock; // Put block at start of chain
    else {
        printf("!x!x!x ATTEMPTING TO SET INDEX TO NULL! !x!x!x\n");
        return;
    }
#ifdef CACHE_DEBUG
    printf("tempBlock's block is now the start of the LRU chain.\n");
#endif

    tempBlock = firstBlock;
#ifdef CACHE_DEBUG
    printf("tempBlock is now pointing to the old front of the LRU chain...");
#endif
    while (tempBlock->nextBlock != cache->blockArray[targetIndex][0].nextBlock) {
        // Run through chain until you find the block that used to be before tempBlock
        tempBlock = tempBlock->nextBlock;
    }
#ifdef CACHE_DEBUG
    printf("and now is pointing to the block that used to be before the new front of the LRU chain\n");
#endif

    struct Block * temp2 = cache->blockArray[targetIndex][0].nextBlock;
#ifdef CACHE_DEBUG
    printf("temp2 is now pointing to the new start of the LRU chain...");
#endif
    tempBlock->nextBlock = temp2->nextBlock;
#ifdef CACHE_DEBUG
    printf("and now the block before doesn't point to the new front, it points to the one that came after it.\n ");
#endif
    temp2->nextBlock = firstBlock;
#ifdef CACHE_DEBUG
    printf("Now the new front points to the old front, and the LRU chain is restored.");
    printf("~~~> <~~~\n\n");
#endif

    return;


}

void printCacheStatus(struct Cache * cache, int * cacheIndexCounter) {
#ifdef CACHE_DEBUG
    printf("*********************************************************\n");
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
#endif
    int i = 0, j = 0;
    for (i = 0; i < cache->lengthOfWay; i++) {
        if (cacheIndexCounter[i] == 1) {
            printf("Index %llx:\t", (unsigned long long) i);
            for (j = 1; j < (cache->associativity + 1); j++) {
                // Skip the first column since it's being used as a dummy pointer
                if (cache->blockArray[i][j].tag != 0) // Block has a tag in it
                    printf("| V: %d, D: %d Tag: \t%llx|", cache->blockArray[i][j].valid, cache->blockArray[i][j].dirty, cache->blockArray[i][j].tag);
                else // Block doesn't have a tag in it
                    printf("| V: %d, D: %d Tag: - |", cache->blockArray[i][j].valid, cache->blockArray[i][j].dirty);
            }
            printf("\n");
        }
    }
    printf("\n");

#ifdef CACHE_POINTER_DEBUG
    printf("~~~> ---------------- Pointer Contents ----------------\n\n");
    int row = 0, col = 0;
    for (row = 0; row < cache->lengthOfWay; row++) {
        if (cacheIndexCounter[row] == 1) {
            printf("~~~> Index: %llx", (unsigned long long) row);
            for (col = 0; col < cache->associativity + 1; col++) {
                struct Block* thisBlock = &cache->blockArray[row][col];
                printf("||This block:%p, points to %p||\t", thisBlock, thisBlock->nextBlock);
            }
            printf("\n");
        }
    }
#endif

#ifdef CACHE_DEBUG
    printf("End of Cache Status\n\n");
    printf("*********************************************************\n");
#endif
}

void freeCache(struct Cache * cache) {
    int row;

    for (row = 0; row < cache->lengthOfWay; row++) {
        free(cache->blockArray[row]);
    }

    free(cache->blockArray);

    free(cache);
}

//void checkDebugStatus() {
//#ifdef CACHE_DEBUG
//    printf("Debugging is turned ON in cache.c\n");
//#endif
//
//#ifndef CACHE_DEBUG
//    printf("Debugging is turned ON in cache.c\n");
//#endif
//
//}