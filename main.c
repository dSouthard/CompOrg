#include <stdio.h>
#include <getopt.h>
#include <math.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>
#include "cache.h"

//#define DEBUG

// Global Variables
// Cache structures
struct Cache *iCache, *dCache, *l2Cache; // L1 I and D caches, L2 Unified cache

// Main memory parameters, taken from project description
int mem_sendaddr = 10;
int mem_ready = 30;
int mem_chunktime = 15;
int mem_chunksize = 8;

// Variables for pulling out values from address fields
unsigned long long address, startAddress, byteAddress, l1EndAddress, l2EndAddress; // Read in new addresses
unsigned long long l1Tag, l2Tag; // Address Tag
unsigned long long l1IndexField, l2IndexField; // Address Index
unsigned long long l1Byte, l2Byte; // Address Byte Field

// Calculate cost of the system
double l1Cost = 0, l2Cost = 0, mainMemoryCost = 0;

unsigned long long traceCounter = 0; // Track total number of traces received
unsigned long long int executionTime = 0; // Track total execution time
unsigned long long int misalignments = 0; // Track total misalignments
unsigned long long flushTime = 0, flushes = 0; // Track time to flush
int flushMax = 380000; // Flush cache every time this value is reached

// Multiple counters for troubleshooting
unsigned long long readCounter = 0; // Track number of read traces received
unsigned long long writeCounter = 0; // Track number of write traces received
unsigned long long instructionCounter = 0; // Track number of instruction traces received
unsigned long long l1HitCounter = 0;
unsigned long long l1MissCounter = 0;
unsigned long long l2HitCounter = 0;
unsigned long long l2MissCounter = 0;

// Helper Function Declarations
void setDebugStatus(int status);
void findFields();
void findNextFields();
double calculateCost(int level, int size, int associativity);
int scanCache(struct Cache* cache, unsigned long long targetTag, unsigned long long targetIndex, char op);
unsigned long long moveBlock(struct Cache* cache, unsigned long long targetTag, unsigned long long targetIndex, int isDirty);
struct Cache * initialize(int newCacheSize, int newBlockSize, int newAssociativity, int newMissTime, int newHitTime);
void printHelpStatement();
void flushCaches(struct Cache * iCache, struct Cache * dCache, struct Cache * l2Cache, int mainMemoryTime, int transferTime, int busWidth);
void printCacheStatus(struct Cache * cache, int * cacheTracker);
void printReport();
void freeCache(struct Cache * cache);

int main(int argc, char* argv[]) {
    //**************************  Local Variables
    // Variables to read in traces
    char opCode; // Track type of trace: R [Read], W[Write], or I[Instruction]
    int byteSize = 0; // Track number of bytes referenced by trace
    FILE * configFile; // File to read in configuration and trace files

    // Debugging flag
    int debug = 0;

    // Variables to calculate cost of accessing memory
    int l1_hit_time = 1, l1_miss_time = 1;
    int l2_hit_time = 5, l2_miss_time = 7;
    int l2_transfer_time = 5, l2_bus_width = 16; // L1 -> L2 access penalties

    // Main memory access penalty
    int mainMemoryTime = mem_sendaddr + mem_ready + (mem_chunktime * 64 / mem_chunksize);

    // Default cache values
    // Use the fact that L1 Data and Instruction caches are always the same size
    int l1size = 8192, l2size = 32768, l1assoc = 1, l2assoc = 1;

#ifndef DEBUG
    // Check input arguments  
    if (argc < 2 || argc > 4) {
        // Put in too many arguments, print help statement
        printHelpStatement();
        return -1;
    }

    if (argc >= 2) { // Configuration file was added
        printf("Configuration: %s\n\n", argv[1]);

        // Read in configFile
        configFile = fopen(argv[1], "r");

        int value = 0;
        char parameter[16];
        while (fscanf(configFile, "%s %d\n", parameter, &value) != EOF) {
            if (!strcmp(parameter, "l1size")) {
                l1size = value;
            }
            if (!strcmp(parameter, "l2size")) {
                l2size = value;
            }
            if (!strcmp(parameter, "l1assoc")) {
                l1assoc = value;
            }
            if (!strcmp(parameter, "l2assoc")) {
                l2assoc = value;
            }
            if (!strcmp(parameter, "memchunksize")) {
                mem_chunksize = value;
            }
        }
    }

    // Finished reading file --> Close it
    fclose(configFile);

    if (argc > 2) { // Flags were added
        int i = 2;
        for (i = 2; i < argc; i++) {
            if (strcmp(argv[i], "-h") == 0) // Print help statement
                printHelpStatement();
            else if (strcmp(argv[i], "-d") == 0) // Start debugging statements
                debug = 1;
        }
    }


#endif

#ifdef DEBUG
    debug = 1;
    configFile = fopen("default_8", "r");
    FILE * inputFile = fopen("tr3", "r");

    int value = 0;
    char parameter[16];
    while (fscanf(configFile, "%s %d\n", parameter, &value) != EOF) {
        if (!strcmp(parameter, "l1size")) {
            l1size = value;
        }
        if (!strcmp(parameter, "l2size")) {
            l2size = value;
        }
        if (!strcmp(parameter, "l1assoc")) {
            l1assoc = value;
        }
        if (!strcmp(parameter, "l2assoc")) {
            l2assoc = value;
        }
        if (!strcmp(parameter, "memchunksize")) {
            mem_chunksize = value;
        }
    }

    // Finished reading file --> Close it
    fclose(configFile);
#endif

    // Initiate caches based on configFile values
    iCache = initialize(l1size, 32, l1assoc, l1_miss_time, l1_hit_time);
    dCache = initialize(l1size, 32, l1assoc, l1_miss_time, l1_hit_time);
    l2Cache = initialize(l2size, 64, l2assoc, l2_miss_time, l2_hit_time);

    // Calculate cost of the system based on configuration sizes
    l1Cost = calculateCost(1, l1size, l1assoc);
    l2Cost = calculateCost(2, l2size, l2assoc);
    int memChunkDivider;
    if (mem_chunksize <= 16)
        memChunkDivider = 1;
    else
        memChunkDivider = mem_chunksize / 16;
    mainMemoryCost = 50 + (memChunkDivider * 25);

    // Set debug status in cache.c
    setDebugStatus(debug);

    // ***************************** Start Reading in the Traces
    unsigned long long writeOverTag; // returned value if a LRU was dirty
    int hit; // Flag to tell if there was a hit in a cache

    // Track which indexes have been changed
    int tracker = 0;

    // iCache tracker
    int iCacheIndexTracker[iCache->lengthOfWay];
    for (tracker = 0; tracker < iCache->lengthOfWay; tracker++) {
        iCacheIndexTracker[tracker] = 0;
    }

    // dCache tracker
    int dCacheIndexTracker[dCache->lengthOfWay];
    for (tracker = 0; tracker < dCache->lengthOfWay; tracker++) {
        dCacheIndexTracker[tracker] = 0;
    }

    // l2Cache tracker
    int l2CacheIndexTracker[l2Cache->lengthOfWay];
    for (tracker = 0; tracker < l2Cache->lengthOfWay; tracker++) {
        l2CacheIndexTracker[tracker] = 0;
    }

#ifndef DEBUG
    while (scanf("%c %Lx %d\n", &opCode, &address, &byteSize) == 3) {
#endif

        if (debug) {
            printf("-------------- Inputting Traces Now -------------\n");
        }
#ifdef DEBUG
        while (fscanf(inputFile, "%c %Lx %d\n", &opCode, &address, &byteSize) != EOF) {
#endif
            traceCounter++; // Increment trace counter
#ifdef DEBUG
            if (traceCounter == 30) {
                printf("\nL1 Instruction Cache current status: \n");
                printCacheStatus(iCache, iCacheIndexTracker);

                printf("\nL1 Data Cache current status: \n");
                printCacheStatus(dCache, dCacheIndexTracker);

                printf("\nL2 Unified Cache current status: \n");
                printCacheStatus(l2Cache, l2CacheIndexTracker);
            }
#endif
            int traceTime = executionTime; // Track individual trace times

            findFields(); // Have pulled out Index, Byte, and Tag fields for L1 cache
            if (debug) {
                printf("\nTrace #%lld --- Address  = %llx, Byte Size = %d, Type = %c \n", traceCounter, address, byteSize, opCode);
                printf("\t *** Start Address = %llx *** \n", startAddress);
                printf("\t *** L1: Index Field = %llx, Tag Field = %llx *** \n", l1IndexField, l1Tag);
            }

            int bytesLeft = byteSize;

            while (bytesLeft) { // Go through and repeat for as many bytes as necessary
                if (opCode == 'I') {
                    instructionCounter++; // Increment instruction counter

                    // Track which indexes have been changed
                    iCacheIndexTracker[l1IndexField] = 1;
                    l2CacheIndexTracker[l2IndexField] = 1;

                    hit = scanCache(iCache, l1Tag, l1IndexField, opCode); // Check L1I cache
                    if (hit) { // Found in L1I cache
                        if (debug)
                            printf("\t HIT: Found in L1, adding L1 hit time (+%d)\n", l1_hit_time);
                        l1HitCounter++; //Increment hit counter
                        executionTime += iCache->hitTime;
                        // 
                    } else { // Not in L1 Cache
                        if (debug) {
                            printf("\t MISS: Not found in L1, adding L1 miss time (+%d)\n", l1_miss_time);
                            printf("\t *** L2: Index Field = %llx, Tag Field = %llx *** \n", l2IndexField, l2Tag);
                        }

                        l1MissCounter++;
                        executionTime += iCache->missTime;

                        // Scan L2
                        hit = scanCache(l2Cache, l2Tag, l2IndexField, opCode);
                        if (hit) {
                            if (debug)
                                printf("\t HIT: Found in L2, adding L2 hit time (+%d)\n", l2_hit_time);
                            l2HitCounter++; // Increment hit counter
                            executionTime += l2Cache->hitTime;

                            // Bring into L1 cache
                            if (debug) {
                                printf("\t Writing block back to L1...");
                                printf("Adding L2->L1 transfer time (+%d)\n", l2_transfer_time * (iCache->blockSize / l2_bus_width));
                                printf("\t Adding L1 replay time (+%d)\n", l1_hit_time);
                            }

                            // Find LRU to write over in L1
                            writeOverTag = moveBlock(iCache, l1Tag, l1IndexField, 0); // MoveBlock needs to be seen as a write request

                            // Time penalty
                            executionTime += l2_transfer_time * (iCache->blockSize / l2_bus_width); // Move desired block from L2->L1
                            executionTime += iCache->hitTime;

                            if (writeOverTag) { // Tag was dirty, needs to be written back to L2
                                if (debug) {
                                    printf("\t Kicking out dirty block in L1...\n");
                                    printf("\t Rewriting dirty block to L2, adding transfer time (+%d)\n", l2_transfer_time * (iCache->blockSize / l2_bus_width));
                                }

                                // Move dirty block back to the L2 cache
                                writeOverTag = moveBlock(l2Cache, writeOverTag, l2IndexField, 1);
                                executionTime += l2_transfer_time * (iCache->blockSize / l2_bus_width); // Move dirty block from L1->L2

                                if (writeOverTag) { // Tag in L2 was dirty, needs to be written to Main Memory
                                    if (debug) {
                                        printf("\t Kicking out dirty block in L2...\n");
                                        printf("\t Rewriting dirty block to main memory (+%d)\n", mainMemoryTime);
                                    }
                                    executionTime += mainMemoryTime; // Move dirty block from L2 -> memory
                                }
                            }

                            //                            if ((byteAddress + bytesLeft) < l1EndAddress) {
                            //                                // Transfer will only take one reference
                            //                                bytesLeft = 0;
                            //                            } else {
                            //                                bytesLeft = l1EndAddress - byteAddress;
                            //                                byteAddress = l1EndAddress;
                            //                                findNextFields();
                            //                            }

                        } else { // Miss in L2 Cache, read from memory
                            if (debug) {
                                printf("\t MISS: Not found in L2, adding L2 miss time (+%d)\n", l2_miss_time);
                                printf("\t Bringing in from main memory (+%d)\n", mainMemoryTime);
                            }
                            l2MissCounter++; // Increment miss counter
                            executionTime += l2Cache->missTime;

                            executionTime += mainMemoryTime; // Bring from memory -> L2

                            // Find LRU to write over in L2
                            writeOverTag = moveBlock(l2Cache, l2Tag, l2IndexField, 0); // MoveBlock needs to be seen as a write request
                            if (writeOverTag) { // LRU in L2 was dirty
                                if (debug) {
                                    printf("\t Kicking out dirty block in L2...\n");
                                    printf("\t Rewriting dirty block to main memory (+%d)\n", mainMemoryTime);
                                }
                                executionTime += mainMemoryTime; // Move dirty block from L2 -> memory
                            }

                            // Find LRU to write over in L1
                            if (debug) {
                                printf("\t Writing block back to L1...");
                                printf("Adding L2->L1 transfer time (+%d)\n", l2_transfer_time * (iCache->blockSize / l2_bus_width));
                                printf("\t Adding L1 replay time (+%d)\n", l1_hit_time);
                            }
                            writeOverTag = moveBlock(iCache, l1Tag, l1IndexField, 0); // MoveBlock needs to be seen as a write request

                            // Time penalty
                            executionTime += l2_transfer_time * (iCache->blockSize / l2_bus_width); // Move desired block from L2->L1
                            executionTime += iCache->hitTime;

                            if (writeOverTag) { // Tag was dirty, needs to be written back to L2
                                if (debug) {
                                    printf("\t Kicking out dirty block in L1...\n");
                                    printf("\t Rewriting dirty block to L2, adding transfer time (+%d)\n", l2_transfer_time * (iCache->blockSize / l2_bus_width));
                                }

                                // write dirty tag back to L2
                                writeOverTag = moveBlock(l2Cache, writeOverTag, l2IndexField, 1);
                                executionTime += l2_transfer_time * (iCache->blockSize / l2_bus_width); // Move dirty block from L1->L2

                                if (writeOverTag) { // Tag in L2 was dirty, needs to be written to Main Memory
                                    if (debug) {
                                        printf("\t Kicking out dirty block in L2...\n");
                                        printf("\t Rewriting dirty block to main memory (+%d)\n", mainMemoryTime);
                                    }
                                    executionTime += mainMemoryTime; // Move dirty block from L2 -> memory
                                }
                            }
                        }
                    }
                } else if (opCode == 'R') {
                    readCounter++; // increment read counter

                    // Track which indexes have been changed
                    dCacheIndexTracker[l1IndexField] = 1;
                    l2CacheIndexTracker[l2IndexField] = 1;

                    hit = scanCache(dCache, l1Tag, l1IndexField, opCode); // Check L1I cache
                    if (hit) { // Found in L1I cache
                        if (debug)
                            printf("\t HIT: Found in L1 \n");
                        l1HitCounter++; //Increment hit counter
                        executionTime += dCache->hitTime;
                    } else { // Not in L1 Cache
                        if (debug) {
                            printf("\t MISS: Not found in L1, adding L1 miss time (+%d)\n", l1_miss_time);
                            printf("\t *** L2: Index Field = %llx, Tag Field = %llx *** \n", l2IndexField, l2Tag);
                        }
                        l1MissCounter++;
                        executionTime += dCache->missTime;

                        // ************************** Scan L2
                        // Reset tag/index fields
                        hit = scanCache(l2Cache, l2Tag, l2IndexField, opCode);
                        if (hit) {
                            if (debug)
                                printf("\t HIT: Found in L2, adding L2 hit time (+%d)\n", l2_hit_time);

                            l2HitCounter++; // Increment hit counter
                            executionTime += l2Cache->hitTime;

                            // Bring into L1 cache
                            // Find LRU to write over in L1 data cache
                            if (debug) {
                                printf("\t Writing block back to L1...");
                                printf("Adding L2->L1 transfer time (+%d)\n", l2_transfer_time * (iCache->blockSize / l2_bus_width));
                                printf("\t Adding L1 replay time (+%d)\n", l1_hit_time);
                            }
                            writeOverTag = moveBlock(dCache, l1Tag, l1IndexField, 0); // MoveBlock needs to be seen as a write request
                            // Time penalty
                            executionTime += l2_transfer_time * (dCache->blockSize / l2_bus_width); // Move desired block from L2->L1
                            executionTime += dCache->hitTime;

                            if (writeOverTag) { // Tag was dirty, needs to be written back to L2
                                if (debug) {
                                    printf("\t Kicking out dirty block in L1...\n");
                                    printf("\t Rewriting dirty block to L2, adding transfer time (+%d)\n", l2_transfer_time * (iCache->blockSize / l2_bus_width));
                                }
                                // Write dirty tag back to L2
                                writeOverTag = moveBlock(l2Cache, writeOverTag, l2IndexField, 1);
                                executionTime += l2_transfer_time * (dCache->blockSize / l2_bus_width); // Move dirty block from L1->L2

                                if (writeOverTag) { // Tag in L2 was dirty, needs to be written to Main Memory
                                    if (debug) {
                                        printf("\t Kicking out dirty block in L2...\n");
                                        printf("\t Rewriting dirty block to main memory (+%d)\n", mainMemoryTime);
                                    }
                                    executionTime += mainMemoryTime; // Move dirty block from L2 -> memory
                                }
                            }

                        } else { // Miss in L2 Cache, read from memory
                            if (debug) {
                                printf("\t MISS: Not found in L2, adding L2 miss time (+%d)\n", l2_miss_time);
                                printf("\t Bringing in from main memory (+%d)\n", mainMemoryTime);
                            }
                            l2MissCounter++; // Increment miss counter
                            executionTime += l2Cache->missTime;
                            executionTime += mainMemoryTime; // Bring from memory -> L2

                            // Find LRU to write over in L2
                            writeOverTag = moveBlock(l2Cache, l2Tag, l2IndexField, 0); // MoveBlock needs to be seen as a write request
                            if (writeOverTag) { // LRU in L2 was dirty
                                if (debug) {
                                    printf("\t Kicking out dirty block in L2...\n");
                                    printf("\t Rewriting dirty block to main memory (+%d)\n", mainMemoryTime);
                                }
                                executionTime += mainMemoryTime; // Move dirty block from L2 -> memory
                            }

                            // Find LRU to write over in L1
                            if (debug) {
                                printf("\t Writing block back to L1...");
                                printf("Adding L2->L1 transfer time (+%d)\n", l2_transfer_time * (iCache->blockSize / l2_bus_width));
                                printf("\t Adding L1 replay time (+%d)\n", l1_hit_time);
                            }
                            writeOverTag = moveBlock(dCache, l1Tag, l1IndexField, 0); // MoveBlock needs to be seen as a write request
                            // Time penalty
                            executionTime += l2_transfer_time * (dCache->blockSize / l2_bus_width); // Move desired block from L2->L1
                            executionTime += dCache->hitTime;

                            if (writeOverTag) { // Tag was dirty, needs to be written back to L2
                                if (debug) {
                                    printf("\t Kicking out dirty block in L1...\n");
                                    printf("\t Rewriting dirty block to L2, adding transfer time (+%d)\n", l2_transfer_time * (iCache->blockSize / l2_bus_width));
                                }
                                // Write dirty tag back to L2
                                writeOverTag = moveBlock(l2Cache, writeOverTag, l2IndexField, 1);
                                executionTime += l2_transfer_time * (dCache->blockSize / l2_bus_width); // Move dirty block from L1->L2

                                if (writeOverTag) { // Tag in L2 was dirty, needs to be written to Main Memory
                                    if (debug) {
                                        printf("\t Kicking out dirty block in L2...\n");
                                        printf("\t Rewriting dirty block to main memory (+%d)\n", mainMemoryTime);
                                    }
                                    executionTime += mainMemoryTime; // Move dirty block from L2 -> memory
                                }
                            }
                        }
                    }
                } else if (opCode == 'W') {
                    writeCounter++; // Increment write counter

                    // Track which indexes have been accessed
                    dCacheIndexTracker[l1IndexField] = 1;
                    l2CacheIndexTracker[l2IndexField] = 1;

                    hit = scanCache(dCache, l1Tag, l1IndexField, opCode); // Check L1I cache
                    if (hit) { // Found in L1I cache
                        if (debug)
                            printf("\t HIT: Found in L1, adding L1 hit time (+%d)\n", l1_hit_time);
                        l1HitCounter++; //Increment hit counter
                        executionTime += dCache->hitTime;
                    } else { // Not in L1 Cache
                        if (debug) {
                            printf("\t MISS: Not found in L1, adding L1 Miss time (+%d)\n", l1_miss_time);
                            printf("\t *** L2: Index Field = %llx, Tag Field = %llx *** \n", l2IndexField, l2Tag);
                        }
                        l1MissCounter++;
                        executionTime += dCache->missTime;

                        // Scan L2
                        hit = scanCache(l2Cache, l2Tag, l2IndexField, opCode);
                        if (hit) {
                            if (debug)
                                printf("\t HIT: Found in L2, adding L2 hit time (+%d)\n", l2_hit_time);
                            l2HitCounter++; // Increment hit counter
                            executionTime += l2Cache->hitTime;

                            // Bring into L1 cache
                            // Find LRU to write over in L1 data cache
                            if (debug) {
                                printf("\t Writing block back to L1...");
                                printf("Adding L2->L1 transfer time (+%d)\n", l2_transfer_time * (iCache->blockSize / l2_bus_width));
                                printf("\t Adding L1 replay time (+%d)\n", l1_hit_time);
                            }
                            writeOverTag = moveBlock(dCache, l1Tag, l1IndexField, 1); // MoveBlock needs to be seen as a write request
                            // Time penalty
                            executionTime += l2_transfer_time * (dCache->blockSize / l2_bus_width); // Move desired block from L2->L1
                            executionTime += dCache->hitTime;

                            if (writeOverTag) { // Tag was dirty, needs to be written back to L2
                                if (debug) {
                                    printf("\t Kicking out dirty block in L1...\n");
                                    printf("\t Rewriting dirty block to L2, adding transfer time (+%d)\n", l2_transfer_time * (iCache->blockSize / l2_bus_width));
                                }
                                // Write dirty tag back to L2
                                writeOverTag = moveBlock(l2Cache, writeOverTag, l2IndexField, 1);
                                executionTime += l2_transfer_time * (dCache->blockSize / l2_bus_width); // Move dirty block from L1->L2

                                if (writeOverTag) { // Tag in L2 was dirty, needs to be written to Main Memory
                                    if (debug) {
                                        printf("\t Kicking out dirty block in L2...\n");
                                        printf("\t Rewriting dirty block to main memory (+%d)\n", mainMemoryTime);
                                    }
                                    executionTime += mainMemoryTime; // Move dirty block from L2 -> memory
                                }
                            }

                        } else { // Miss in L2 Cache, read from memory
                            if (debug) {
                                printf("\t MISS: Not found in L2, adding L2 miss time (+%d)\n", l2_miss_time);
                                printf("\t Bringing in from main memory (+%d)\n", mainMemoryTime);
                                printf("\t Adding L2 Replay Time (+%d)\n", l2_hit_time);
                            }
                            l2MissCounter++; // Increment miss counter
                            executionTime += l2Cache->missTime;
                            executionTime += mainMemoryTime; // Bring from memory -> L2

                            // Find LRU to write over in L2
                            writeOverTag = moveBlock(l2Cache, l2Tag, l2IndexField, 0); // MoveBlock needs to be seen as a write request
                            if (writeOverTag) { // LRU in L2 was dirty
                                if (debug) {
                                    printf("\t Kicking out dirty block in L2...\n");
                                    printf("\t Rewriting dirty block to main memory (+%d)\n", mainMemoryTime);
                                    printf("\t Adding L2 Replay Time (+%d)\n", l2_hit_time);
                                }
                                executionTime += mainMemoryTime; // Move dirty block from L2 -> memory
                            }

                            // Find LRU to write over in L1
                            if (debug) {
                                printf("\t Writing block back to L1...");
                                printf("Adding L2->L1 transfer time (+%d)\n", l2_transfer_time * (iCache->blockSize / l2_bus_width));
                                printf("\t Adding L1 replay time (+%d)\n", l1_hit_time);
                            }
                            writeOverTag = moveBlock(dCache, l1Tag, l1IndexField, 1); // MoveBlock needs to be seen as a write request
                            // Time penalty
                            executionTime += l2_transfer_time * (dCache->blockSize / l2_bus_width); // Move desired block from L2->L1
                            executionTime += dCache->hitTime;

                            if (writeOverTag) { // Tag was dirty, needs to be written back to L2
                                if (debug) {
                                    printf("\t Kicking out dirty block in L1...\n");
                                    printf("\t Rewriting dirty block to L2, adding transfer time (+%d)\n", l2_transfer_time * (iCache->blockSize / l2_bus_width));
                                }
                                // Write dirty tag back to L2
                                writeOverTag = moveBlock(l2Cache, writeOverTag, l2IndexField, 1);
                                executionTime += l2_transfer_time * (dCache->blockSize / l2_bus_width); // Move dirty block from L1->L2

                                if (writeOverTag) { // Tag in L2 was dirty, needs to be written to Main Memory
                                    if (debug) {
                                        printf("\t Kicking out dirty block in L2...\n");
                                        printf("\t Rewriting dirty block to main memory (+%d)\n", mainMemoryTime);
                                    }
                                    executionTime += mainMemoryTime; // Move dirty block from L2 -> memory
                                }
                            }
                        }
                    }
                }

                // Find the next fields if you need to move on to the next block
                if ((byteAddress + bytesLeft) < l1EndAddress) {
                    // Transfer will only take one reference
                    bytesLeft = 0;
                } else {
                    bytesLeft = l1EndAddress - byteAddress;
                    byteAddress = l1EndAddress;
                    findNextFields();
                }
            }

            // Finished reading a trace, be ready to read in the next trace
            if (traceCounter % flushMax == 0) { // Reached the point where we need to flush the caches
                if (debug) {
                    printf("Time to flush the caches! \n");
                }
                flushes++; // increment flush counter
                flushCaches(iCache, dCache, l2Cache, mainMemoryTime, l2_transfer_time, l2_bus_width);
            }

            if (debug) {
                printf("\t ---> Total trace time: %llu\n", executionTime - traceTime);
                printf("\t ---> Total execution time: %llu\n", executionTime);
                printf("-------------------- End of Trace --------------------\n");
            }

#ifdef DEBUG
        }
        fclose(inputFile);
        flushCaches(iCache, dCache, l2Cache, mainMemoryTime, l2_transfer_time, l2_bus_width);
        flushes++;
#endif

#ifndef DEBUG
    }
#endif
    printReport();

    printf("---------------------------------------------------\n");
    printf("Cache Final Contents: \n\n"
            "Memory Level: L1 Instruction Cache\n");
    printCacheStatus(iCache, iCacheIndexTracker);

    printf("\nMemory Level: L1 Data Cache\n");
    printCacheStatus(dCache, dCacheIndexTracker);

    printf("\nMemory Level: L2 Unified Cache \n");
    printCacheStatus(l2Cache, l2CacheIndexTracker);

    // Free all allocated memory
    freeCache(iCache);
    freeCache(dCache);
    freeCache(l2Cache);

    return 0;
}

double calculateCost(int level, int size, int associativity) {
    double result;
    int i = 0;
    if (level == 1) { // L1 Cache
        result = 100 * (size / 4096); // L1 cache memory costs $100 for each 4KB
        // Add an additional $100 for each doubling in associativity beyond direct-mapped
        for (i = 0; i < size / 4096; i++) {
            result += 100 * log(associativity) / log(2);
        }
    } else if (level == 2) { // L2 Cache
        if (size < 65536) // Check if L2 memory is < 64Kb (still need to pay for full cost)
            result = 50;
        else
            result = 100; // L2 cache memory costs $50 per 64KB
        // Add an additional $50 for each doubling in associativity
        result += 50 * log(associativity) / log(2);
    }
    return result;
}

void findFields() {
    // Pull out the byte field from the address
    // Fill space with 1's, then shift over by byeOffset
    l1Byte = (~0) << iCache->byteOffsetSize;
    l2Byte = (~0) << l2Cache->byteOffsetSize;

    startAddress = address & (~3); // Find start address for this block
    l1EndAddress = startAddress + (iCache->blockSize - 1);

    l1Byte = ~l1Byte; // Switch all 1's to 0's
    l1Byte = l1Byte & address; // Mask out all fields but the incoming byte field

    l2Byte = ~l2Byte; // Switch all 1's to 0's
    l2Byte = l2Byte & address; // Mask out all fields but the incoming byte field

    byteAddress = address;  // Keep track of start of desired bytes
    
    // Pull out the index field from the address
    // Fill space with 1's, then shift over by byeOffset
    l1IndexField = (~0) << (iCache->indexFieldSize + iCache->byteOffsetSize);
    l2IndexField = (~0) << (l2Cache->indexFieldSize + l2Cache->byteOffsetSize);
    l1Tag = l1IndexField; // Save correct placing of 1's for finding the tag field next
    l1IndexField = ~l1IndexField; // Switch all 1's to 0's
    l1IndexField = l1IndexField & address; // Mask out all fields but the incoming byte/index field

    l2Tag = l2IndexField; // Save correct placing of 1's for finding the tag field next
    l2IndexField = ~l2IndexField; // Switch all 1's to 0's
    l2IndexField = l2IndexField & address; // Mask out all fields but the incoming byte/index field

    // Shift out the byte field
    l1IndexField = l1IndexField >> iCache->byteOffsetSize;
    l2IndexField = l2IndexField >> l2Cache->byteOffsetSize;

    // Pull out the tag field
    l1Tag = l1Tag & address;
    l2Tag = l2Tag & address;

    // Shift out other fields
    l1Tag = l1Tag >> (iCache->indexFieldSize + iCache->byteOffsetSize);
    l2Tag = l2Tag >> (l2Cache->indexFieldSize + l2Cache->byteOffsetSize);

    return;
}

void findNextFields() {
    // Pull out the byte field from the address
    // Fill space with 1's, then shift over by byeOffset
    l1Byte = (~0) << iCache->byteOffsetSize;
    l2Byte = (~0) << l2Cache->byteOffsetSize;

    l1Byte = ~l1Byte; // Switch all 1's to 0's
    l1Byte = l1Byte & byteAddress; // Mask out all fields but the incoming byte field

    l2Byte = ~l2Byte; // Switch all 1's to 0's
    l2Byte = l2Byte & byteAddress; // Mask out all fields but the incoming byte field

    // Pull out the index field from the address
    // Fill space with 1's, then shift over by byeOffset
    l1IndexField = (~0) << (iCache->indexFieldSize + iCache->byteOffsetSize);
    l2IndexField = (~0) << (l2Cache->indexFieldSize + l2Cache->byteOffsetSize);
    l1Tag = l1IndexField; // Save correct placing of 1's for finding the tag field next
    l1IndexField = ~l1IndexField; // Switch all 1's to 0's
    l1IndexField = l1IndexField & byteAddress; // Mask out all fields but the incoming byte/index field

    l2Tag = l2IndexField; // Save correct placing of 1's for finding the tag field next
    l2IndexField = ~l2IndexField; // Switch all 1's to 0's
    l2IndexField = l2IndexField & byteAddress; // Mask out all fields but the incoming byte/index field

    // Shift out the byte field
    l1IndexField = l1IndexField >> iCache->byteOffsetSize;
    l2IndexField = l2IndexField >> l2Cache->byteOffsetSize;

    // Pull out the tag field
    l1Tag = l1Tag & byteAddress;
    l2Tag = l2Tag & byteAddress;

    // Shift out other fields
    l1Tag = l1Tag >> (iCache->indexFieldSize + iCache->byteOffsetSize);
    l2Tag = l2Tag >> (l2Cache->indexFieldSize + l2Cache->byteOffsetSize);

    return;
}

void printHelpStatement() {
    printf("Memory Simulation Project, ECEN 4593 \n"
            "Usage: cat <traces> | ./cachesim <config_file> -FLAGS\n"
            "Configuration File: Used to set up caches. \n\n"
            "Flag Options:\n"
            "\t -h: Print up usage statement\n"
            "\t -d: Turn on debugging statements");
}

void printReport() {
    printf("---------------------------------------------------\n");
    printf("\t\t\t\t Simulation Results\n");
    printf("---------------------------------------------------\n");
    printf("Memory System:\n");
    printf("\t D Cache: Size = %d, \tAssociativity = %d, \tBlock size = %d\n", dCache->cacheSize, dCache->associativity, dCache->blockSize);
    printf("\t I Cache: Size = %d, \tAssociativity = %d, \tBlock size = %d\n", iCache->cacheSize, iCache->associativity, iCache->blockSize);
    printf("\t L2 Unified Cache: Size = %d, \tAssociativity = %d, \tBlock size = %d\n", l2Cache->cacheSize, l2Cache->associativity, l2Cache->blockSize);
    printf("\t Main Memory: Ready time = %d, \tChunksize = %d, \tChunktime = %d,\n\n", mem_ready, mem_chunksize, mem_chunktime);

    printf("Execution Time = %d, \tTotal Traces Read = %d\n", (int) executionTime, (int) traceCounter);
    printf("Flush time = %llu\n\n", flushTime);

    printf("Number of Reference Types: \t[Percentage] \n"
            "\tReads \t= \t%llu \t\t[%.1f%%]\n"
            "\tWrites \t= \t%llu  \t\t[%.1f%%]\n"
            "\tInst. \t= \t%llu \t\t[%.1f%%]\n"
            "\tTotal \t= \t%llu\n\n",
            readCounter, (double) readCounter * 100 / traceCounter,
            writeCounter, (double) writeCounter * 100 / traceCounter,
            instructionCounter, (double) instructionCounter * 100 / traceCounter,
            readCounter + writeCounter + instructionCounter);

    unsigned long long readCycles = (iCache->readRefs * iCache->readTime) + (dCache->readRefs * dCache->readTime) + (l2Cache->readRefs * l2Cache->readTime);
    unsigned long long writeCycles = (iCache->writeRefs * iCache->writeTime) + (dCache->writeRefs * dCache->writeTime) + (l2Cache->writeRefs * l2Cache->writeTime);
    unsigned long long insCycles = (iCache->insRefs * iCache->instructionTime) + (dCache->insRefs * dCache->instructionTime) + (l2Cache->insRefs * l2Cache->instructionTime);

    printf("Total Cycles for Activities: \t[Percentage] \n"
            "\tReads \t= \t%llu \t\t[%.1f%%]\n"
            "\tWrites \t= \t%llu \t\t[%.1f%%]\n"
            "\tInst. \t= \t%llu \t\t[%.1f%%]\n"
            "\tTotal \t= \t%llu\n\n",
            readCycles, (double) readCycles * 100 / executionTime,
            writeCycles, (double) writeCycles * 100 / executionTime,
            insCycles, (double) insCycles * 100 / executionTime,
            readCycles + writeCycles + insCycles);

    if (readCounter)
        readCycles = readCycles / readCounter;
    else
        readCycles = 0;

    if (writeCounter)
        writeCycles = writeCycles / writeCounter;
    else
        writeCycles = 0;
    if (instructionCounter)
        insCycles = insCycles / instructionCounter;
    else
        insCycles = 0;

    printf("Average Cycles for Activities: \n"
            "\tReads = %.1f, \tWrites = %.1f, \tInstructions = %.1f\n\n",
            (double) readCycles, (double) writeCycles, (double) insCycles);

    printf("Ideal: Execution Time = xxxxx; \tCPI = xx.xx\n");
    printf("Ideal Misaligned Execution Time = xxxxxx; \tCPI = xx.xx\n\n");

    unsigned long long totalRequests = iCache->hits + iCache->misses;
    printf("Memory Level: L1 Instruction Cache\n"
            "\tHit Count = %llu, \t Miss Count = %llu\n", iCache->hits, iCache->misses);
    printf("\tTotal Requests = %llu\n", totalRequests);
    printf("\tHit Rate: %.1f%%, \t Miss Rate = %.1f%%\n", (double) iCache->hits * 100 / totalRequests, (double) iCache->misses * 100 / totalRequests);
    printf("\tKickouts = %llu, \t Dirty-kickouts = %llu, \t Transfers = %llu\n",
            iCache->kickouts, iCache->dirtyKickouts, iCache->transfers);
    printf("\tFlush Kickouts = %llu\n\n", iCache->flushKickouts);

    totalRequests = dCache->hits + dCache->misses;
    printf("Memory Level: L1 Data Cache\n"
            "\tHit Count = %llu, \t Miss Count = %llu\n", dCache->hits, dCache->misses);
    printf("\tTotal Requests = %llu\n", totalRequests);
    printf("\tHit Rate: %.1f%%, \t Miss Rate = %.1f%%\n", (double) dCache->hits * 100 / totalRequests, (double) dCache->misses * 100 / totalRequests);
    printf("\tKickouts = %llu, \t Dirty-kickouts = %llu, \t Transfers = %llu\n",
            dCache->kickouts, dCache->dirtyKickouts, dCache->transfers);
    printf("\tFlush Kickouts = %llu\n\n", dCache->flushKickouts);

    totalRequests = l2Cache->hits + l2Cache->misses;
    printf("Memory Level: L2 Cache\n"
            "\tHit Count = %llu, \t Miss Count = %llu\n", l2Cache->hits, l2Cache->misses);
    printf("\tTotal Requests = %llu\n", totalRequests);
    printf("\tHit Rate: %.1f%%, \t Miss Rate = %.1f%%\n", (double) l2Cache->hits * 100 / totalRequests, (double) l2Cache->misses * 100 / totalRequests);
    printf("\tKickouts = %llu, \t Dirty-kickouts = %llu, \t Transfers = %llu\n",
            l2Cache->kickouts, l2Cache->dirtyKickouts, l2Cache->transfers);
    printf("\tFlush Kickouts = %llu\n\n", l2Cache->flushKickouts);

    printf("\nSystem Costs:\n");
    printf("\t L1 Cache Costs (Instruction Cache $%d + Data Cache $%d) = $%d\n", (int) l1Cost, (int) l1Cost, (int) l1Cost * 2);
    printf("\t L2 Cache Costs: $%d \n", (int) l2Cost);
    printf("\t Main Memory Costs: $%d \n", (int) mainMemoryCost);
    printf("\t Total Memory Cost: $%d \n\n", (int) (mainMemoryCost + l1Cost * 2 + l2Cost));

    printf("Flushes = %llu, Invalidates = %llu\n\n", flushes, iCache->invalidates + dCache->invalidates + l2Cache->invalidates);

    printf("---------------------------------------------------\n");

}
