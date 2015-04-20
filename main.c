#include <stdio.h>
#include <getopt.h>
#include <math.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>
#include "cache.h"

#define DEBUG

// Global Variables
// Cache structures
struct Cache *icache, *dcache, *l2cache; // L1 I and D caches, L2 Unified cache

// Main memory parameters, taken from project description
int mem_sendaddr = 10;
int mem_ready = 30;
int mem_chunktime = 15;
int mem_chunksize = 8;

// Variables for pulling out values from address fields
unsigned long long address[10]; // Read in new addresses
unsigned long long l1Tag[10], l2Tag[10]; // Address Tag
unsigned long long l1IndexField[10], l2IndexField[10]; // Address Index
unsigned long long l1Byte, l2Byte; // Address Byte Field

// Calculate cost of the system
double l1Cost = 0, l2Cost = 0, mainMemoryCost = 0;

unsigned long long traceCounter = 0; // Track total number of traces received
unsigned long long int executionTime = 0; // Track total execution time
unsigned long long int misalignments = 0; // Track total misalignments
unsigned long long flushTime = 0; // Track time to flush

// Multiple counters for troubleshooting
unsigned long long readCounter = 0; // Track number of read traces received
unsigned long long writeCounter = 0; // Track number of write traces received
unsigned long long instructionCounter = 0; // Track number of instruction traces received
unsigned long long l1HitCounter = 0;
unsigned long long l1MissCounter = 0;
unsigned long long l2HitCounter = 0;
unsigned long long l2MissCounter = 0;

// Helper Function Declarations
void findFields();
double calculateCost(int level, int size, int associativity);
int scanCache(struct Cache* cache, unsigned long long targetTag, unsigned long long targetIndex, char op);
unsigned long long moveBlock(struct Cache* cache, unsigned long long targetTag, unsigned long long targetIndex);
struct Cache * initialize(int newCacheSize, int newBlockSize, int newAssociativity, int newMissTime, int newHitTime);
void printHelpStatement();
void printSetup();
void printCacheStatus(struct Cache * cache, int * cacheTracker);
void printReport();

int main(int argc, char* argv[]) {
    //**************************  Local Variables
    // Variables to read in traces
    char opCode; // Track type of trace: R [Read], W[Write], or I[Instruction]
    int byteSize = 0; // Track number of bytes referenced by trace
    FILE * configFile, * inputFile; // File to read in configuration and trace files

    // Debugging flag
    int verbose = 0;

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
        // Didn't input Configuration file or put in too many arguments, print help statement
        printHelpStatement();
        return -1;
    }


    if (argc > 2) { // Flags were added
        int i = 2;
        for (i = 2; i < argc; i++) {
            if (strcmp(argv[i], "-h") == 0) // Print help statement
                printHelpStatement();
            else if (strcmp(argv[i], "-v") == 0) // Start debugging statements
                verbose = 1;
        }
    }

    // Read in configFile
    configFile = fopen(argv[1], "r");
#endif

#ifdef DEBUG
    verbose = 1;
    configFile = fopen("all-4way_8", "r");
    inputFile = fopen("tr3", "r");
#endif

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

    // Initiate caches based on configFile values
    icache = initialize(l1size, 32, l1assoc, l1_miss_time, l1_hit_time);
    dcache = initialize(l1size, 32, l1assoc, l1_miss_time, l1_hit_time);
    l2cache = initialize(l2size, 64, l2assoc, l2_miss_time, l2_hit_time);

    // Calculate cost of the system based on configuration sizes
    l1Cost = calculateCost(1, l1size, l1assoc);
    l2Cost = calculateCost(2, l2size, l2assoc);
    int memChunkDivider;
    if (mem_chunksize <= 16)
        memChunkDivider = 1;
    else
        memChunkDivider = mem_chunksize / 16;
    mainMemoryCost = 50 + (memChunkDivider * 25);

    // ***************************** Start Reading in the Traces
    unsigned long long writeOverTag[10]; // returned value if a LRU was dirty
    int hit; // Flag to tell if there was a hit in a cache

    // Track which indexes have been changed
    int tracker = 0;

    // icache
    int icacheIndexTracker[icache->lengthOfWay];

    for (tracker = 0; tracker < icache->lengthOfWay; tracker++) {
        icacheIndexTracker[tracker] = 0;
    }

    // dcache
    int dcacheIndexTracker[dcache->lengthOfWay];
    for (tracker = 0; tracker < dcache->lengthOfWay; tracker++) {
        dcacheIndexTracker[tracker] = 0;
    }

    // l2cache
    int l2cacheIndexTracker[l2cache->lengthOfWay];
    for (tracker = 0; tracker < l2cache->lengthOfWay; tracker++) {
        l2cacheIndexTracker[tracker] = 0;
    }

#ifndef DEBUG
    while (scanf("%c %Lx %d\n", &opCode, &address[0], &byteSize) == 3) {
#endif

        if (verbose) {
            printf("-------------- Inputting Traces Now -------------\n");
        }
#ifdef DEBUG
        while (fscanf(inputFile, "%c %Lx %d\n", &opCode, &address[0], &byteSize) != EOF) {
#endif
            traceCounter++; // Increment trace counter
            int traceTime = 0; // Track individual trace times

            findFields(); // Have pulled out Index, Byte, and Tag fields for L1 cache
            if (verbose) {
                printf("\nTrace #%lld --- Address  = %llx, Byte Size = %d, Type = %c \n", traceCounter, address[0], byteSize, opCode);
                printf("\t *** L1: Index Field = %llx, Tag Field = %llx *** \n", l1IndexField[0], l1Tag[0]);
            }

            if (opCode == 'I') {
                instructionCounter++; // Increment instruction counter
                icache->readRefs++; // instruction must access L1 instruction cache

                // Track which indexes have been changed
                icacheIndexTracker[l1IndexField[0]] = 1;
                l2cacheIndexTracker[l2IndexField[0]] = 1;

                hit = scanCache(icache, l1Tag[0], l1IndexField[0], opCode); // Check L1I cache
                if (hit) { // Found in L1I cache
                    if (verbose)
                        printf("\t HIT: Found in L1, adding L1 hit time (+%d)\n", l1_hit_time);
                    l1HitCounter++; //Increment hit counter
                    executionTime += icache->hitTime;
                    traceTime += icache->hitTime;
                } else { // Not in L1 Cache
                    if (verbose) {
                        printf("\t MISS: Not found in L1, adding L1 miss time (+%d)\n", l1_miss_time);
                        printf("\t *** L2: Index Field = %llx, Tag Field = %llx *** \n", l2IndexField[0], l2Tag[0]);
                    }
                    l1MissCounter++;
                    executionTime += icache->missTime;
                    traceTime += icache->missTime;
                    // Scan L2
                    hit = scanCache(l2cache, l2Tag[0], l2IndexField[0], opCode);
                    if (hit) {
                        if (verbose)
                            printf("\t HIT: Found in L2, adding L2 hit time (+%d)\n", l2_hit_time);
                        l2HitCounter++; // Increment hit counter
                        executionTime += l2cache->hitTime;
                        traceTime += l2cache->hitTime;
                        // Bring into L1 cache
                        // Find LRU to write over in L1
                        writeOverTag[0] = moveBlock(icache, l1Tag[0], l1IndexField[0]); // MoveBlock needs to be seen as a write request

                        if (verbose) {
                            printf("\t Writing block back to L1...");
                            printf("Adding L2->L1 transfer time (+%d)\n", l2_transfer_time * (icache->blockSize * 8 / l2_bus_width));
                            printf("\t Adding L1 replay time (+%d)\n", l1_hit_time);
                        }

                        // Time penalty
                        executionTime += l2_transfer_time * (icache->blockSize * 8 / l2_bus_width); // Move desired block from L2->L1
                        executionTime += icache->hitTime;

                        traceTime += l2_transfer_time * (icache->blockSize * 8 / l2_bus_width); // Move desired block from L2->L1
                        traceTime += icache->hitTime;

                        if (writeOverTag[0]) { // Tag was dirty, needs to be written back to L2
                            if (verbose) {
                                printf("\t Kicking out dirty block in L1...\n");
                                printf("\t Rewriting dirty block to L2, adding transfer time (+%d)\n", l2_transfer_time * (icache->blockSize * 8 / l2_bus_width));
                            }
                            writeOverTag[0] = moveBlock(l2cache, writeOverTag[0], l2IndexField[0]);
                            executionTime += l2_transfer_time * (icache->blockSize * 8 / l2_bus_width); // Move dirty block from L1->L2
                            traceTime += l2_transfer_time * (icache->blockSize * 8 / l2_bus_width); // Move dirty block from L1->L2

                            if (writeOverTag[0]) { // Tag in L2 was dirty, needs to be written to Main Memory
                                if (verbose) {
                                    printf("\t Kicking out dirty block in L2...\n");
                                    printf("\t Rewriting dirty block to main memory (+%d)\n", mainMemoryTime);
                                }
                                executionTime += mainMemoryTime; // Move dirty block from L2 -> memory
                                traceTime += mainMemoryTime; // Move dirty block from L2 -> memory
                            }
                        }

                    } else { // Miss in L2 Cache, read from memory
                        if (verbose) {
                            printf("\t MISS: Not found in L2, adding L2 miss time (+%d)\n", l2_miss_time);
                            printf("\t Bringing in from main memory (+%d)\n", mainMemoryTime);
                        }
                        l2MissCounter++; // Increment miss counter
                        executionTime += l2cache->missTime;
                        traceTime += l2cache->missTime;

                        executionTime += mainMemoryTime; // Bring from memory -> L2
                        traceTime += mainMemoryTime; // Bring from memory -> L2

                        // Find LRU to write over in L2
                        writeOverTag[0] = moveBlock(l2cache, l2Tag[0], l2IndexField[0]); // MoveBlock needs to be seen as a write request
                        if (writeOverTag[0]) { // LRU in L2 was dirty
                            if (verbose) {
                                printf("\t Kicking out dirty block in L2...\n");
                                printf("\t Rewriting dirty block to main memory (+%d)\n", mainMemoryTime);
                            }
                            executionTime += mainMemoryTime; // Move dirty block from L2 -> memory
                            traceTime += mainMemoryTime; // Move dirty block from L2 -> memory
                        }

                        // Find LRU to write over in L1
                        if (verbose) {
                            printf("\t Writing block back to L1...");
                            printf("Adding L2->L1 transfer time (+%d)\n", l2_transfer_time * (icache->blockSize * 8 / l2_bus_width));
                            printf("\t Adding L1 replay time (+%d)\n", l1_hit_time);
                        }
                        writeOverTag[0] = moveBlock(icache, l1Tag[0], l1IndexField[0]); // MoveBlock needs to be seen as a write request
                        // Time penalty

                        executionTime += l2_transfer_time * (icache->blockSize * 8 / l2_bus_width); // Move desired block from L2->L1
                        executionTime += icache->hitTime;

                        traceTime += l2_transfer_time * (icache->blockSize * 8 / l2_bus_width); // Move desired block from L2->L1
                        traceTime += icache->hitTime;

                        if (writeOverTag[0]) { // Tag was dirty, needs to be written back to L2
                            if (verbose) {
                                printf("\t Kicking out dirty block in L1...\n");
                                printf("\t Rewriting dirty block to L2, adding transfer time (+%d)\n", l2_transfer_time * (icache->blockSize * 8 / l2_bus_width));
                            }
                            writeOverTag[0] = moveBlock(l2cache, writeOverTag[0], l2IndexField[0]);
                            executionTime += l2_transfer_time * (icache->blockSize * 8 / l2_bus_width); // Move dirty block from L1->L2
                            traceTime += l2_transfer_time * (icache->blockSize * 8 / l2_bus_width); // Move dirty block from L1->L2

                            if (writeOverTag[0]) { // Tag in L2 was dirty, needs to be written to Main Memory
                                if (verbose) {
                                    printf("\t Kicking out dirty block in L2...\n");
                                    printf("\t Rewriting dirty block to main memory (+%d)\n", mainMemoryTime);
                                }
                                executionTime += mainMemoryTime; // Move dirty block from L2 -> memory
                                traceTime += mainMemoryTime; // Move dirty block from L2 -> memory
                            }
                        }
                    }
                }
            } else if (opCode == 'R') {
                dcache->readRefs++; // Read must access L1 data cache
                readCounter++; // increment read counter

                // Track which indexes have been changed
                dcacheIndexTracker[l1IndexField[0]] = 1;
                l2cacheIndexTracker[l2IndexField[0]] = 1;

                hit = scanCache(dcache, l1Tag[0], l1IndexField[0], opCode); // Check L1I cache
                if (hit) { // Found in L1I cache
                    if (verbose)
                        printf("\t HIT: Found in L1 \n");
                    l1HitCounter++; //Increment hit counter
                    executionTime += dcache->hitTime;
                    traceTime += dcache->hitTime;
                } else { // Not in L1 Cache
                    if (verbose) {
                        printf("\t MISS: Not found in L1, adding L1 miss time (+%d)\n", l1_miss_time);
                        printf("\t *** L2: Index Field = %llx, Tag Field = %llx *** \n", l2IndexField[0], l2Tag[0]);
                    }
                    l1MissCounter++;
                    executionTime += dcache->missTime;
                    traceTime += dcache->missTime;

                    // ************************** Scan L2
                    // Reset tag/index fields
                    hit = scanCache(l2cache, l2Tag[0], l2IndexField[0], opCode);
                    if (hit) {
                        if (verbose)
                            printf("\t HIT: Found in L2, adding L2 hit time (+%d)\n", l2_hit_time);

                        l2HitCounter++; // Increment hit counter
                        executionTime += l2cache->hitTime;
                        traceTime += l2cache->hitTime;

                        // Bring into L1 cache
                        // Find LRU to write over in L1 data cache
                        if (verbose) {
                            printf("\t Writing block back to L1...");
                            printf("Adding L2->L1 transfer time (+%d)\n", l2_transfer_time * (icache->blockSize * 8 / l2_bus_width));
                            printf("\t Adding L1 replay time (+%d)\n", l1_hit_time);
                        }
                        writeOverTag[0] = moveBlock(dcache, l1Tag[0], l1IndexField[0]); // MoveBlock needs to be seen as a write request
                        // Time penalty
                        executionTime += l2_transfer_time * (dcache->blockSize * 8 / l2_bus_width); // Move desired block from L2->L1
                        executionTime += dcache->hitTime;

                        traceTime += l2_transfer_time * (dcache->blockSize * 8 / l2_bus_width); // Move desired block from L2->L1
                        traceTime += dcache->hitTime;

                        if (writeOverTag[0]) { // Tag was dirty, needs to be written back to L2
                            if (verbose) {
                                printf("\t Kicking out dirty block in L1...\n");
                                printf("\t Rewriting dirty block to L2, adding transfer time (+%d)\n", l2_transfer_time * (icache->blockSize * 8 / l2_bus_width));
                            }
                            writeOverTag[0] = moveBlock(l2cache, writeOverTag[0], l2IndexField[0]);
                            executionTime += l2_transfer_time * (dcache->blockSize * 8 / l2_bus_width); // Move dirty block from L1->L2
                            traceTime += l2_transfer_time * (dcache->blockSize * 8 / l2_bus_width); // Move dirty block from L1->L2

                            if (writeOverTag[0]) { // Tag in L2 was dirty, needs to be written to Main Memory
                                if (verbose) {
                                    printf("\t Kicking out dirty block in L2...\n");
                                    printf("\t Rewriting dirty block to main memory (+%d)\n", mainMemoryTime);
                                }
                                executionTime += mainMemoryTime; // Move dirty block from L2 -> memory
                                traceTime += mainMemoryTime; // Move dirty block from L2 -> memory
                            }
                        }

                    } else { // Miss in L2 Cache, read from memory
                        if (verbose) {
                            printf("\t MISS: Not found in L2, adding L2 miss time (+%d)\n", l2_miss_time);
                            printf("\t Bringing in from main memory (+%d)\n", mainMemoryTime);
                        }
                        l2MissCounter++; // Increment miss counter
                        executionTime += l2cache->missTime;
                        executionTime += mainMemoryTime; // Bring from memory -> L2

                        traceTime += l2cache->missTime;
                        traceTime += mainMemoryTime; // Bring from memory -> L2

                        // Find LRU to write over in L2
                        writeOverTag[0] = moveBlock(l2cache, l2Tag[0], l2IndexField[0]); // MoveBlock needs to be seen as a write request
                        if (writeOverTag[0]) { // LRU in L2 was dirty
                            if (verbose) {
                                printf("\t Kicking out dirty block in L2...\n");
                                printf("\t Rewriting dirty block to main memory (+%d)\n", mainMemoryTime);
                            }
                            executionTime += mainMemoryTime; // Move dirty block from L2 -> memory
                            traceTime += mainMemoryTime; // Move dirty block from L2 -> memory
                        }

                        // Find LRU to write over in L1
                        if (verbose) {
                            printf("\t Writing block back to L1...");
                            printf("Adding L2->L1 transfer time (+%d)\n", l2_transfer_time * (icache->blockSize * 8 / l2_bus_width));
                            printf("\t Adding L1 replay time (+%d)\n", l1_hit_time);
                        }
                        writeOverTag[0] = moveBlock(dcache, l1Tag[0], l2IndexField[0]); // MoveBlock needs to be seen as a write request
                        // Time penalty
                        executionTime += l2_transfer_time * (dcache->blockSize * 8 / l2_bus_width); // Move desired block from L2->L1
                        executionTime += dcache->hitTime;

                        traceTime += l2_transfer_time * (dcache->blockSize * 8 / l2_bus_width); // Move desired block from L2->L1
                        traceTime += dcache->hitTime;

                        if (writeOverTag[0]) { // Tag was dirty, needs to be written back to L2
                            if (verbose) {
                                printf("\t Kicking out dirty block in L1...\n");
                                printf("\t Rewriting dirty block to L2, adding transfer time (+%d)\n", l2_transfer_time * (icache->blockSize * 8 / l2_bus_width));
                            }
                            writeOverTag[0] = moveBlock(l2cache, writeOverTag[0], l2IndexField[0]);
                            executionTime += l2_transfer_time * (dcache->blockSize * 8 / l2_bus_width); // Move dirty block from L1->L2
                            traceTime += l2_transfer_time * (dcache->blockSize * 8 / l2_bus_width); // Move dirty block from L1->L2

                            if (writeOverTag[0]) { // Tag in L2 was dirty, needs to be written to Main Memory
                                if (verbose) {
                                    printf("\t Kicking out dirty block in L2...\n");
                                    printf("\t Rewriting dirty block to main memory (+%d)\n", mainMemoryTime);
                                }
                                executionTime += mainMemoryTime; // Move dirty block from L2 -> memory
                                traceTime += mainMemoryTime; // Move dirty block from L2 -> memory
                            }
                        }
                    }
                }
            } else if (opCode == 'W') {
                writeCounter++; // Increment write counter
                dcache->writeRefs++; // write must access L1 data cache

                // Track which indexes have been accessed
                dcacheIndexTracker[l1IndexField[0]] = 1;
                l2cacheIndexTracker[l2IndexField[0]] = 1;

                hit = scanCache(dcache, l1Tag[0], l1IndexField[0], opCode); // Check L1I cache
                if (hit) { // Found in L1I cache
                    if (verbose)
                        printf("\t HIT: Found in L1, adding L1 hit time (+%d)\n", l1_hit_time);
                    l1HitCounter++; //Increment hit counter
                    executionTime += dcache->hitTime;
                    traceTime += dcache->hitTime;
                } else { // Not in L1 Cache
                    if (verbose) {
                        printf("\t MISS: Not found in L1, adding L1 Miss time (+%d)\n", l1_miss_time);
                        printf("\t *** L2: Index Field = %llx, Tag Field = %llx *** \n", l2IndexField[0], l2Tag[0]);
                    }
                    l1MissCounter++;
                    executionTime += dcache->missTime;
                    traceTime += dcache->missTime;

                    // Scan L2
                    hit = scanCache(l2cache, l2Tag[0], l2IndexField[0], opCode);
                    if (hit) {
                        if (verbose)
                            printf("\t HIT: Found in L2, adding L2 hit time (+%d)\n", l2_hit_time);
                        l2HitCounter++; // Increment hit counter
                        executionTime += l2cache->hitTime;
                        traceTime += l2cache->hitTime;

                        // Bring into L1 cache
                        // Find LRU to write over in L1 data cache
                        if (verbose) {
                            printf("\t Writing block back to L1...");
                            printf("Adding L2->L1 transfer time (+%d)\n", l2_transfer_time * (icache->blockSize * 8 / l2_bus_width));
                            printf("\t Adding L1 replay time (+%d)\n", l1_hit_time);
                        }
                        writeOverTag[0] = moveBlock(dcache, l1Tag[0], l1IndexField[0]); // MoveBlock needs to be seen as a write request
                        // Time penalty
                        executionTime += l2_transfer_time * (dcache->blockSize * 8 / l2_bus_width); // Move desired block from L2->L1
                        executionTime += dcache->hitTime;

                        traceTime += l2_transfer_time * (dcache->blockSize * 8 / l2_bus_width); // Move desired block from L2->L1
                        traceTime += dcache->hitTime;

                        if (writeOverTag[0]) { // Tag was dirty, needs to be written back to L2
                            if (verbose) {
                                printf("\t Kicking out dirty block in L1...\n");
                                printf("\t Rewriting dirty block to L2, adding transfer time (+%d)\n", l2_transfer_time * (icache->blockSize * 8 / l2_bus_width));
                            }
                            writeOverTag[0] = moveBlock(l2cache, writeOverTag[0], l2IndexField[0]);
                            executionTime += l2_transfer_time * (dcache->blockSize * 8 / l2_bus_width); // Move dirty block from L1->L2
                            traceTime += l2_transfer_time * (dcache->blockSize * 8 / l2_bus_width); // Move dirty block from L1->L2

                            if (writeOverTag[0]) { // Tag in L2 was dirty, needs to be written to Main Memory
                                if (verbose) {
                                    printf("\t Kicking out dirty block in L2...\n");
                                    printf("\t Rewriting dirty block to main memory (+%d)\n", mainMemoryTime);
                                }
                                executionTime += mainMemoryTime; // Move dirty block from L2 -> memory
                                traceTime += mainMemoryTime; // Move dirty block from L2 -> memory
                            }
                        }

                    } else { // Miss in L2 Cache, read from memory
                        if (verbose) {
                            printf("\t MISS: Not found in L2, adding L2 miss time (+%d)\n", l2_miss_time);
                            printf("\t Bringing in from main memory (+%d)\n", mainMemoryTime);
                        }
                        l2MissCounter++; // Increment miss counter
                        executionTime += l2cache->missTime;
                        executionTime += mainMemoryTime; // Bring from memory -> L2

                        traceTime += l2cache->missTime;
                        traceTime += mainMemoryTime; // Bring from memory -> L2

                        // Find LRU to write over in L2
                        writeOverTag[0] = moveBlock(l2cache, l2Tag[0], l2IndexField[0]); // MoveBlock needs to be seen as a write request
                        if (writeOverTag[0]) { // LRU in L2 was dirty
                            if (verbose) {
                                printf("\t Kicking out dirty block in L2...\n");
                                printf("\t Rewriting dirty block to main memory (+%d)\n", mainMemoryTime);
                            }
                            executionTime += mainMemoryTime; // Move dirty block from L2 -> memory
                            traceTime += mainMemoryTime; // Move dirty block from L2 -> memory
                        }

                        // Find LRU to write over in L1
                        if (verbose) {
                            printf("\t Writing block back to L1...");
                            printf("Adding L2->L1 transfer time (+%d)\n", l2_transfer_time * (icache->blockSize * 8 / l2_bus_width));
                            printf("\t Adding L1 replay time (+%d)\n", l1_hit_time);
                        }
                        writeOverTag[0] = moveBlock(dcache, l1Tag[0], l1IndexField[0]); // MoveBlock needs to be seen as a write request
                        // Time penalty
                        executionTime += l2_transfer_time * (dcache->blockSize * 8 / l2_bus_width); // Move desired block from L2->L1
                        executionTime += dcache->hitTime;

                        traceTime += l2_transfer_time * (dcache->blockSize * 8 / l2_bus_width); // Move desired block from L2->L1
                        traceTime += dcache->hitTime;

                        if (writeOverTag[0]) { // Tag was dirty, needs to be written back to L2
                            if (verbose) {
                                printf("\t Kicking out dirty block in L1...\n");
                                printf("\t Rewriting dirty block to L2, adding transfer time (+%d)\n", l2_transfer_time * (icache->blockSize * 8 / l2_bus_width));
                            }
                            writeOverTag[0] = moveBlock(l2cache, writeOverTag[0], l2IndexField[0]);
                            executionTime += l2_transfer_time * (dcache->blockSize * 8 / l2_bus_width); // Move dirty block from L1->L2
                            traceTime += l2_transfer_time * (dcache->blockSize * 8 / l2_bus_width); // Move dirty block from L1->L2

                            if (writeOverTag[0]) { // Tag in L2 was dirty, needs to be written to Main Memory
                                if (verbose) {
                                    printf("\t Kicking out dirty block in L2...\n");
                                    printf("\t Rewriting dirty block to main memory (+%d)\n", mainMemoryTime);
                                }
                                executionTime += mainMemoryTime; // Move dirty block from L2 -> memory
                                traceTime += mainMemoryTime; // Move dirty block from L2 -> memory
                            }
                        }
                    }
                }
            }
#ifdef DEBUG
            if (traceCounter == 20) {
                printf("\nL1 Instruction Cache current status: \n");
                printCacheStatus(icache, icacheIndexTracker);

                printf("\nL1 Data Cache current status: \n");
                printCacheStatus(dcache, dcacheIndexTracker);

                printf("\nL2 Unified Cache current status: \n");
                printCacheStatus(l2cache, l2cacheIndexTracker);
            }
            printf("\t ---> Total trace time: %d\n", traceTime);
            printf("\t ---> Total execution time: %d\n", (int) executionTime);
            printf("-------------------- End of Trace --------------------\n");
        }
#endif

#ifndef DEBUG
    }
#endif

    if (verbose) {
        printf("\nL1 Instruction Cache current status: \n");
        printCacheStatus(icache, icacheIndexTracker);

        printf("\nL1 Data Cache current status: \n");
        printCacheStatus(dcache, dcacheIndexTracker);

        printf("\nL2 Unified Cache current status: \n");
        printCacheStatus(l2cache, l2cacheIndexTracker);
    }

    printSetup();
    printReport();
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
            size = 65536;
        result = 50 * (size / 65536); // L2 cache memory costs $50 per 64KB
        // Add additional $50 for each doubling in associativity
        result += 50 * log(associativity) / log(2);
    }
    return result;
}

void findFields() {
    // Pull out the byte field from the address
    // Fill space with 1's, then shift over by byeOffset
    l1Byte = (~0) << icache->byteOffsetSize;
    l2Byte = (~0) << l2cache->byteOffsetSize;

    l1Byte = ~l1Byte; // Switch all 1's to 0's
    l1Byte = l1Byte & address[0]; // Mask out all fields but the incoming byte field

    l2Byte = ~l2Byte; // Switch all 1's to 0's
    l2Byte = l2Byte & address[0]; // Mask out all fields but the incoming byte field

    // Pull out the index field from the address
    // Fill space with 1's, then shift over by byeOffset
    l1IndexField[0] = (~0) << (icache->indexFieldSize + icache->byteOffsetSize);
    l2IndexField[0] = (~0) << (l2cache->indexFieldSize + l2cache->byteOffsetSize);
    l1Tag[0] = l1IndexField[0]; // Save correct placing of 1's for finding the tag field next
    l1IndexField[0] = ~l1IndexField[0]; // Switch all 1's to 0's
    l1IndexField[0] = l1IndexField[0] & address[0]; // Mask out all fields but the incoming byte/index field

    l2Tag[0] = l2IndexField[0]; // Save correct placing of 1's for finding the tag field next
    l2IndexField[0] = ~l2IndexField[0]; // Switch all 1's to 0's
    l2IndexField[0] = l2IndexField[0] & address[0]; // Mask out all fields but the incoming byte/index field

    // Shift out the byte field
    l1IndexField[0] = l1IndexField[0] >> icache->byteOffsetSize;
    l2IndexField[0] = l2IndexField[0] >> l2cache->byteOffsetSize;

    // Pull out the tag field
    l1Tag[0] = l1Tag[0] & address[0];
    l2Tag[0] = l2Tag[0] & address[0];

    // Shift out other fields
    l1Tag[0] = l1Tag[0] >> (icache->indexFieldSize + icache->byteOffsetSize);
    l2Tag[0] = l2Tag[0] >> (l2cache->indexFieldSize + l2cache->byteOffsetSize);

    return;
}

void printHelpStatement() {
    printf("Memory Simulation Project, ECEN 4593 \n"
            "Usage: cat <traces> | ./cachesim <config_file> -FLAGS\n"
            "Configuration File: Used to set up caches. \n\n"
            "Flag Options:\n"
            "\t -h: Print up usage statement\n"
            "\t -v: Turn on debugging statements");
}

void printSetup() {
    printf("Memory System:\n");
    printf("---------------------------------------------------\n");
    printf("\t L1: Data Cache: Size = %d, Associativity = %d : Block size = %d\n", dcache->cacheSize, dcache->associativity, dcache->blockSize);
    printf("\t\t Instruction Cache: Size = %d, Associativity = %d : Block size = %d\n", icache->cacheSize, icache->associativity, icache->blockSize);

    printf("\t L2 Unified Cache: Size = %d, Associativity = %d : Block size = %d\n", l2cache->cacheSize, l2cache->associativity, l2cache->blockSize);

    printf("\t Main Memory Parameters: \n"
            "\t\t Time to send the address to memory = %d,\n"
            "\t\t Time for the memory to be ready for start of transfer = %d,\n"
            "\t\t Time to send/receive a single bus-width of data = %d,\n"
            "\t\t Width of the bus interface to memory (bytes) = %d,\n",
            mem_sendaddr, mem_ready, mem_chunktime, mem_chunksize);

    printf("\n\tSystem Costs:\n");
    printf("\t\t L1: Instruction Cache (%.2lf) + Data Cache(%.2lf) = %.2lf total\n", l1Cost, l1Cost, l1Cost * 2);
    printf("\t\t L2 Cache Costs: %.2lf \n", l2Cost);
    printf("\t\t Main Memory Costs: %.2lf \n", mainMemoryCost);
    printf("\t\tTotal Memory Cost: %0.2lf \n\n\n", mainMemoryCost + l1Cost * 2 + l2Cost);
}

void printReport() {
    printf("Execution Time = %llx, Total Traces Read = %llx\n", executionTime, traceCounter);
    printf("Instruction Counters: Read = %llx, Write = %llx, Instruction = %llx\n", readCounter, writeCounter, instructionCounter);
    printf("Flush time = %llx, Misalignments = %llx\n", flushTime, misalignments);
}