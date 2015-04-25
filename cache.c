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

int debugFlag = 0, pointerFlag = 0;

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
    cache->flushTime = 0;
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
        }

        // Initialize last block to null
        cache->blockArray[i][newAssociativity].nextBlock = NULL; // Point to null
        cache->blockArray[i][newAssociativity].valid = 0;
        cache->blockArray[i][newAssociativity].dirty = 0;
        cache->blockArray[i][newAssociativity].tag = 0;
    }

    // Done
    return cache;
}
//
///********************* Read Trace Function ***************************/

unsigned long long moveBlock(struct Cache* cache, unsigned long long targetTag, unsigned long long targetIndex, int isDirty) {
    cache->transfers++; // Transferring data into cache

    struct Block *tempBlock; // Used for LRU policy implementation

    // Point to the first block in the index
    tempBlock = cache->blockArray[targetIndex][0].nextBlock;

    while (tempBlock->nextBlock != NULL && tempBlock->valid) { // Find the LRU
        tempBlock = tempBlock->nextBlock;
    }

    if ((!tempBlock->valid) || (!tempBlock->dirty)) {
        if (debugFlag) {
            printf("~~~> Block was invalid or clean: good to overwrite.\n");
        }

        if (tempBlock->valid && !(tempBlock->dirty)) {
            cache->kickouts++; // Kicking out a previously valid, clean block
        }

        // Block is invalid or clean, feel free to write over it
//        printf("tempblock's tag is now: %llx\n", tempBlock->tag);
//        printf("~~~> Targettag = %llx\n", targetTag);
        tempBlock->tag = targetTag;
//        printf("tempblock's tag is now: %llx\n", tempBlock->tag);
        tempBlock->valid = 1;

        // May have been written back due to a dirty/flush kick-out
        if (isDirty) {
            tempBlock->dirty = 1; // Needs to be written as dirty
        }

        cache->writeRefs++; // Seen as a write request
        cache->writeTime += cache->hitTime;

        // Update LRU chain
        // Move block to front of LRU chain
        if (cache->blockArray[targetIndex][0].nextBlock != tempBlock) {
            updateLRU(cache, tempBlock, targetIndex);
        }
        return 0;
    }

    if (debugFlag) {
        printf("~~~> Block was dirty, need to return overwritten tag...");
    }

    // If you made it to this point, you're pointing at the last block in the chain
    // All blocks before it would have been valid, and this block is dirty
    unsigned long long returnedTag = tempBlock->tag; // track written over dirty tag
    cache->dirtyKickouts++;
    cache->writeRefs++;
    cache->writeTime += cache->hitTime;

    tempBlock->tag = targetTag; // Put in new targetTag into block

    // Update LRU chain
    // Move block to front of LRU chain
    if (cache->blockArray[targetIndex][0].nextBlock != tempBlock) {
        updateLRU(cache, tempBlock, targetIndex);
    }

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
                if (cache->blockArray[targetIndex][0].nextBlock != tempBlock) {
                    updateLRU(cache, tempBlock, targetIndex);
                }
                return 1;

            } else { // Tag didn't match
                tempBlock = tempBlock->nextBlock;
            }
        } else {
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

    return 0;
}

// Purpose: move tempBlock to the front of the LRU chain with index targetIndex

void updateLRU(struct Cache* cache, struct Block * tempBlock, unsigned long long targetIndex) {
    // firstBlock points to the old start of the chain
    struct Block * firstBlock = cache->blockArray[targetIndex][0].nextBlock;

    cache->blockArray[targetIndex][0].nextBlock = tempBlock; // Put block at start of chain

    tempBlock = firstBlock;
    while (tempBlock->nextBlock != cache->blockArray[targetIndex][0].nextBlock) {
        // Run through chain until you find the block that used to be before tempBlock
        tempBlock = tempBlock->nextBlock;
    }
    struct Block * temp2 = cache->blockArray[targetIndex][0].nextBlock; // Point tempBlock at start of chain
    tempBlock->nextBlock = temp2->nextBlock;    // 
    temp2->nextBlock = firstBlock;
    return;
}

void printCacheStatus(struct Cache * cache, int * cacheIndexCounter) {
    if (debugFlag) {
        printf("*********************************************************\n");
        // Cache variables not already printed in printSetup function at beginning
        printf("Cache length: %d, index field size: %d, byte field size: %d\n",
                cache->lengthOfWay, cache->indexFieldSize, cache->byteOffsetSize);

        printf("Number of hits: %llu, misses: %llu, kick-outs: %llu, dirty kick-outs: %llu\n",
                cache->hits, cache->misses, cache->kickouts, cache->dirtyKickouts);

        printf("Number of flush kick-outs: %llu, invalidates: %llu, flush time: %llu\n",
                cache->flushKickouts, cache->invalidates, cache->flushTime);

        printf("Reference Counts: writes = %llu, reads = %llu, instructions = %llu\n",
                cache->writeRefs, cache->readRefs, cache->insRefs);

        printf("Times: writes = %llu, reads = %llu, instructions = %llu\n",
                cache->writeTime, cache->readTime, cache->instructionTime);

        // Go through each row of the cache
        printf("------ Current Row Status ------\n");
    }
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

    if (pointerFlag) {
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
    }

    if (debugFlag) {
        printf("End of Cache Status\n\n");
        printf("*********************************************************\n");
    }
}

void freeCache(struct Cache * cache) {
    int row;

    for (row = 0; row < cache->lengthOfWay; row++) {
        free(cache->blockArray[row]);
    }

    free(cache->blockArray);

    free(cache);
}

void flushCaches(struct Cache * iCache, struct Cache * dCache, struct Cache * l2Cache, int mainMemoryTime, int transferTime, int busWidth) {
    // Go through L1 block, flush dirty blocks to L2 Cache
    struct Block * tempBlock;
    unsigned long long addressTemp, indexTemp, tagTemp; // Used if L2 needs to have a tag pushed back

    int index;
    if (debugFlag) {
        printf("~~~> Starting to flush the iCache\n");
    }
    for (index = 0; index < iCache->lengthOfWay; index++) {
        tempBlock = iCache->blockArray[index][0].nextBlock;
        // Start pointing at the beginning of the LRU chain and go all the way through
        while (tempBlock != NULL) {
            // Found a valid block
            if (tempBlock->valid) {
                tempBlock->valid = 0;
                iCache->invalidates++; // increment invalidate counter
//                if (debugFlag) {
//                    printf("Block is now invalid, moving on\n");
//                }

                // Check if it was also dirty
                if (tempBlock->dirty) {
//                    if (debugFlag) {
//                        printf("~~~> Block was dirty, incrementing iCache flush kickouts\n");
//                    }

                    iCache->flushKickouts++;

                    // Get ready to write it back to the L2 Cache
                    // Start with 0-filled variables
                    tagTemp = 0;
                    indexTemp = 0;
                    addressTemp = 0;

                    tagTemp = tempBlock->tag; // Get the L1 tag and shift it over to the right spot
                    tagTemp = tagTemp << (iCache->indexFieldSize + iCache->byteOffsetSize);

                    indexTemp = index; // Get the L1 index and shift it over to the right spot
                    indexTemp = indexTemp << iCache->byteOffsetSize;

                    addressTemp = indexTemp + tagTemp; // Combine the two for the original address, minus the byte size

                    // Pull out the L2 index/tag
                    indexTemp = (~0) << (l2Cache->indexFieldSize + l2Cache->byteOffsetSize);
                    tagTemp = indexTemp;
                    indexTemp = ~indexTemp;
                    indexTemp = indexTemp & addressTemp;
                    indexTemp = indexTemp >> iCache->byteOffsetSize; // Pull out the L2 index

                    tagTemp = tagTemp & addressTemp;
                    tagTemp = tagTemp >> (iCache->indexFieldSize + iCache->byteOffsetSize); // Pull out the L2 tag

                    // Ready to write back to the L2 cache
                    addressTemp = 0; // Reset to 0

//                    if (debugFlag) {
//                        printf("~~~> Writing dirtyBlock back to L2 cache.\n");
//                    }
                    addressTemp = moveBlock(l2Cache, tagTemp, indexTemp, 1);
                    iCache->flushTime += transferTime * (iCache->blockSize / busWidth); // Move dirty block from L1->L2

                    if (addressTemp) {
                        // Kicked out another block, need to add more to the flush time
//                        if (debugFlag) printf("~~~> Kicked out another dirty block in L2 cache, writing it back to main memory");
                        l2Cache->flushKickouts++; // Increment L2 flushKickouts
                        iCache->flushTime += mainMemoryTime;
                    }
                }
            }
            tempBlock = tempBlock->nextBlock;
        }
    }

    // Have completed flushing the iCache, repeat for the dCache
    if (debugFlag) {
        printf("~~~> FINISHED FLUSHING ICACHE!!! \n\n"
                "~~~> Starting to flush the dCache\n");
    }
    for (index = 0; index < dCache->lengthOfWay; index++) {
        tempBlock = dCache->blockArray[index][0].nextBlock;
        // Start pointing at the beginning of the LRU chain and go all the way through
        while (tempBlock != NULL) {
            // Found a valid block
            if (tempBlock->valid) {
                tempBlock->valid = 0;
                dCache->invalidates++; // increment invalidate counter
//                if (debugFlag) {
//                    printf("Block is now invalid, moving on\n");
//                }

                // Check to see if it was also dirty
                if (tempBlock->dirty) {
//                    if (debugFlag) {
//                        printf("~~~> Block was dirty, incrementing dCache flush kickouts\n");
//                    }

                    dCache->flushKickouts++;

                    // Get ready to write it back to the L2 Cache
                    // Start with 0-filled variables
                    tagTemp = 0;
                    indexTemp = 0;
                    addressTemp = 0;

                    tagTemp = tempBlock->tag; // Get the L1 tag and shift it over to the right spot
                    tagTemp = tagTemp << (dCache->indexFieldSize + dCache->byteOffsetSize);

                    indexTemp = index; // Get the L1 index and shift it over to the right spot
                    indexTemp = indexTemp << dCache->byteOffsetSize;

                    addressTemp = indexTemp + tagTemp; // Combine the two for the original address, minus the byte size

                    // Pull out the L2 index/tag
                    indexTemp = (~0) << (l2Cache->indexFieldSize + l2Cache->byteOffsetSize);
                    tagTemp = indexTemp;
                    indexTemp = ~indexTemp;
                    indexTemp = indexTemp & addressTemp;
                    indexTemp = indexTemp >> l2Cache->byteOffsetSize; // Pull out the L2 index

                    tagTemp = tagTemp & addressTemp;
                    tagTemp = tagTemp >> (l2Cache->indexFieldSize + l2Cache->byteOffsetSize); // Pull out the L2 tag

                    // Ready to write back to the L2 cache
                    addressTemp = 0; // Reset to 0

//                    if (debugFlag) {
//                        printf("~~~> Writing dirtyBlock back to L2 cache.\n");
//                    }
                    addressTemp = moveBlock(l2Cache, tagTemp, indexTemp, 1);
                    dCache->flushTime += transferTime * (dCache->blockSize / busWidth); // Move dirty block from L1->L2

                    if (addressTemp) {
                        // Kicked out another block, need to add more to the flush time
//                        if (debugFlag) printf("~~~> Kicked out another dirty block in L2 cache, writing it back to main memory");
                        l2Cache->flushKickouts++; // Increment L2 flushKickouts
                        dCache->flushTime += mainMemoryTime;
                    }
                }
            }
            tempBlock = tempBlock->nextBlock;
        }
    }

    // Have completed flushing the dCache, repeat for the l2Cache
    if (debugFlag) {
        printf("~~~> FINISHED FLUSHING DCACHE!!! \n\n"
                "~~~> Starting to flush the l2Cache\n");
    }
    for (index = 0; index < l2Cache->lengthOfWay; index++) {
        tempBlock = l2Cache->blockArray[index][0].nextBlock;
        // Start pointing at the beginning of the LRU chain and go all the way through
        while (tempBlock != NULL) {
            // Found a valid block
            if (tempBlock->valid) {
                tempBlock->valid = 0;
                l2Cache->invalidates++; // increment invalidate counter
//                if (debugFlag) {
//                    printf("Block is now invalid, moving on\n");
//                }

                // Check to see if it was also dirty
                if (tempBlock->dirty) {
//                    if (debugFlag) {
//                        printf("~~~> Block was dirty, incrementing l2Cache flush kickouts\n");
//                    }

                    l2Cache->flushKickouts++;
                    l2Cache->flushTime += mainMemoryTime; // Write it back to main memory
                }
            }
            tempBlock = tempBlock->nextBlock;
        }
    }

    if (debugFlag) {
        printf("~~~> L2 Cache flushed, all caches flushed, all blocks invalidated\n"
                "~~~> Completed flushCache function");
    }
}

void setDebugStatus(int status) {
    debugFlag = status;
    return;
}