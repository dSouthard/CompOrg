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
unsigned long long address, cacheStartAddress, byteStartAddress, byteEndAddress, desiredByte, cacheEndAddress; // Read in new addresses
unsigned long long l1Tag, l2Tag, l2WritebackTag, l2WritebackIndex; // Address Tag
unsigned long long l1IndexField, l2IndexField; // Address Index
unsigned long long l1Byte, l2Byte; // Address Byte Field

// Calculate cost of the system
double l1Cost = 0, l2Cost = 0, mainMemoryCost = 0;

unsigned long long traceCounter = 0; // Track total number of traces received
unsigned long long readTime = 0, writeTime = 0, instructionTime = 0; // Track total number of traces received
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
void writeBack(unsigned long long writeBackTag);

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
    FILE * inputFile = fopen("traces-short/tr6", "r");

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
            int traceTime = executionTime; // Track individual trace times
            int counted = 0; // Only count each trace once

            byteStartAddress = address & (~3); // Find start address for desired word
            byteEndAddress = byteStartAddress + 4; // Find the end address for desired word

            findFields(); // Have pulled out Index, Byte, and Tag fields for L1 cache
            int bytesLeft = byteSize;
            if (debug) printf("\nTrace #%lld --- Address  = %llx, Byte Size = %d, Type = %c \n", traceCounter, address, byteSize, opCode);

            while (bytesLeft > 0) { // Go through and repeat for as many bytes as necessary

                if (debug) {
                    printf("\t Requested Cache Block Start/End address = %llx, \t%llx\n", cacheStartAddress, cacheEndAddress);
                    printf("\t *** Byte Start/End Address = %llx, \t %llx *** \n", byteStartAddress, byteEndAddress);
                    printf("\t *** Desired Byte Address = %llx *** \n", address);
                    printf("\t *** L1: Index Field = %llx, Tag Field = %llx *** \n", l1IndexField, l1Tag);
                }

                if (opCode == 'I') {
                    if (!counted) {
                        instructionCounter++; // Increment instruction counter
                        counted = 1;
                    }

                    // Track which indexes have been changed
                    iCacheIndexTracker[l1IndexField] = 1;
                    l2CacheIndexTracker[l2IndexField] = 1;

                    hit = scanCache(iCache, l1Tag, l1IndexField, opCode); // Check L1I cache
                    if (hit == 1) { // Found in L1I cache
                        if (debug) printf("\t HIT: Found in L1, adding L1 hit time (+%d)\n", l1_hit_time);
                        l1HitCounter++; //Increment hit counter
                        executionTime += iCache->hitTime;
                    } else { // Not in L1 Cache, all tags were valid, LRU is dirty
                        if (debug) {
                            printf("\t MISS: Not found in L1, adding L1 miss time (+%d)\n", l1_miss_time);
                        }

                        l1MissCounter++;
                        executionTime += iCache->missTime;

                        // Find LRU to write over in L1
                        writeOverTag = moveBlock(iCache, l1Tag, l1IndexField, 0); // Tag is now in L1, do accounting details


                        if (writeOverTag) {
                            if (debug) printf("\t Need to write over a dirty block\n");
                            writeBack(writeOverTag); // Reconstruct fields for L2 write-back

                            if (debug) printf("\t L2 Writeback Index = %llx, Tag = %llx\n", l2WritebackIndex, l2WritebackTag);

                            if (scanCache(l2Cache, l2WritebackTag, l2WritebackIndex, 'W')) { // Be seen as a write
                                if (debug) {
                                    printf("\t\t HIT: Found in L2, adding L2 hit time\n");
                                    printf("Adding L1->L2 transfer time (+%d)\n", l2_transfer_time * (iCache->blockSize / l2_bus_width));
                                }
                                executionTime += l2Cache->hitTime;
                                executionTime += l2_transfer_time * (iCache->blockSize / l2_bus_width); // Move desired block from L1 -> L2
                                iCache->instructionTime += l2_transfer_time * (iCache->blockSize / l2_bus_width); // Add to instruction time counter

                            } else {
                                printf("\t\t MISS: Not Found in L2, adding L2 miss time\n");
                                executionTime += l2Cache->missTime;

                                // Find LRU to write over in L2
                                writeOverTag = moveBlock(l2Cache, l2WritebackTag, l2WritebackIndex, 1); // Mark as dirty

                                if (writeOverTag) {
                                    printf("\t\t Need to write over a dirty block\n");
                                    printf("\t\t Adding L2-> memory time (+%d)\n", mainMemoryTime);
                                    executionTime += mainMemoryTime;
                                }

                                if (debug) printf("Bringing block into L2, adding memory -> L2 time (+%d)\n", mainMemoryTime);
                                executionTime += mainMemoryTime;

                                if (debug) printf("Adding L2 replay time (+%d)\n", l2Cache->hitTime);
                                executionTime += l2Cache->hitTime;

                                if (debug) printf("Adding L1->L2 transfer time (+%d)\n", l2_transfer_time * (iCache->blockSize / l2_bus_width));
                                executionTime += l2_transfer_time * (iCache->blockSize / l2_bus_width); // Move desired block from L1 -> L2
                                iCache->instructionTime += l2_transfer_time * (iCache->blockSize / l2_bus_width); // Add to instruction time counter
                            }
                        }

                        // Scan L2
                        if (debug) printf("Now scanning L2 for target tag\n"
                                "\t *** L2: Index Field = %llx, Tag Field = %llx *** \n", l2IndexField, l2Tag);
                        if (scanCache(l2Cache, l2Tag, l2IndexField, opCode)) { // Hit in L2
                            if (debug)
                                printf("\t HIT: Found in L2, adding L2 hit time (+%d)\n", l2_hit_time);
                            l2HitCounter++; // Increment hit counter
                            executionTime += l2Cache->hitTime;

                        } else { // Miss in L2 Cache, read from memory
                            if (debug) {
                                printf("\t MISS: Not found in L2, adding L2 miss time (+%d)\n", l2_miss_time);
                            }

                            executionTime += l2Cache->missTime;

                            // Find LRU to write over in L2
                            writeOverTag = moveBlock(l2Cache, l2Tag, l2IndexField, 0); // Tag is now in L2

                            if (writeOverTag) { // LRU in L2 was dirty
                                if (debug) {
                                    printf("\t Kicking out dirty block in L2: Tag = %llx \n", writeOverTag);
                                    printf("\t Rewriting dirty block L2 -> main memory (+%d)\n", mainMemoryTime);
                                }
                                executionTime += mainMemoryTime; // Move dirty block from L2 -> memory
                            }

                            // Bring block into L2
                            if (debug) {
                                printf("\t Bringing block back to L2, adding main memory -> L2 time (+%d)\n", mainMemoryTime);
                                printf("\t Adding L2 replay time (+%d)\n", l2Cache->hitTime);
                            }
                            executionTime += mainMemoryTime; // Move dirty block from L2 -> memory
                            executionTime += l2Cache->hitTime;
                        }

                        // Bring into L1 cache
                        if (debug) {
                            printf("\t Writing block back to L1...");
                            printf("Adding L2->L1 transfer time (+%d)\n", l2_transfer_time * (iCache->blockSize / l2_bus_width));
                            printf("\t Adding L1 replay time (+%d)\n", l1_hit_time);
                        }

                        // Time penalty
                        executionTime += l2_transfer_time * (iCache->blockSize / l2_bus_width); // Move desired block from L1 -> L2
                        iCache->instructionTime += l2_transfer_time * (iCache->blockSize / l2_bus_width); // Add to instruction time counter
                        executionTime += iCache->hitTime;
                    }
                } else if (opCode == 'R') {
                    if (!counted) {
                        readCounter++; // increment read counter
                        counted = 1;
                    }

                    // Track which indexes have been changed
                    dCacheIndexTracker[l1IndexField] = 1;
                    l2CacheIndexTracker[l2IndexField] = 1;

                    hit = scanCache(dCache, l1Tag, l1IndexField, opCode); // Check L1I cache
                    if (hit == 1) { // Found in L1I cache
                        if (debug)
                            printf("\t HIT: Found in L1, adding L1 hit time (+%d)\n", l1_hit_time);
                        l1HitCounter++; //Increment hit counter
                        executionTime += dCache->hitTime;
                        // 
                    } else { // Not in L1 Cache, all tags were valid, LRU is dirty
                        if (debug) {
                            printf("\t MISS: Not found in L1, adding L1 miss time (+%d)\n", l1_miss_time);
                        }

                        l1MissCounter++;
                        executionTime += dCache->missTime;

                        // Find LRU to write over in L1
                        //                        if (traceCounter == 25) {
                        //                            printf("Trace 25 Tag= %llx, Index =  %llx\n",l1Tag, l1IndexField);
                        //                        }
                        //                        
                        writeOverTag = moveBlock(dCache, l1Tag, l1IndexField, 0); // Tag is now in L1, do accounting details

                        //                        if (traceCounter == 25) {
                        //                            printf("SHOULD BE HERE NOW\n");
                        //                            printCacheStatus(dCache, dCacheIndexTracker);
                        //                            printCacheStatus(l2Cache, l2CacheIndexTracker);
                        //                        }
                        if (writeOverTag) {
                            if (debug) printf("\t Need to write over a dirty block\n");
                            writeBack(writeOverTag); // Reconstruct fields for L2 write-back

                            if (debug) printf("\t L2 Writeback Index = %llx, Tag = %llx\n", l2WritebackIndex, l2WritebackTag);

                            if (scanCache(l2Cache, l2WritebackTag, l2WritebackIndex, 'W')) { // Be seen as a write
                                if (debug) {
                                    printf("\t\t HIT: Found in L2, adding L2 hit time\n");
                                    printf("Adding L1->L2 transfer time (+%d)\n", l2_transfer_time * (dCache->blockSize / l2_bus_width));
                                }
                                executionTime += l2Cache->hitTime;
                                executionTime += l2_transfer_time * (dCache->blockSize / l2_bus_width); // Move desired block from L1 -> L2
                            } else {
                                if (debug) printf("\t\t MISS: Not Found in L2, adding L2 miss time\n");
                                executionTime += l2Cache->missTime;

                                // Find LRU to write over in L2
                                writeOverTag = moveBlock(l2Cache, l2WritebackTag, l2WritebackIndex, 1); // Mark as dirty

                                if (writeOverTag) {
                                    if (debug) {
                                        printf("\t\t Need to write over a dirty block\n");
                                        printf("\t\t Adding L2-> memory time (+%d)\n", mainMemoryTime);
                                    }
                                    executionTime += mainMemoryTime;
                                }

                                if (debug) printf("Bringing block into L2, adding memory -> L2 time (+%d)\n", mainMemoryTime);
                                executionTime += mainMemoryTime;

                                if (debug) printf("Adding L2 replay time (+%d)\n", l2Cache->hitTime);
                                executionTime += l2Cache->hitTime;

                                if (debug) printf("Adding L1->L2 transfer time (+%d)\n", l2_transfer_time * (iCache->blockSize / l2_bus_width));
                                executionTime += l2_transfer_time * (iCache->blockSize / l2_bus_width); // Move desired block from L1 -> L2
                            }
                        }

                        // Scan L2
                        if (debug) printf("Now scanning L2 for target tag\n"
                                "\t *** L2: Index Field = %llx, Tag Field = %llx *** \n", l2IndexField, l2Tag);
                        if (scanCache(l2Cache, l2Tag, l2IndexField, opCode)) { // Hit in L2
                            if (debug)
                                printf("\t HIT: Found in L2, adding L2 hit time (+%d)\n", l2_hit_time);
                            l2HitCounter++; // Increment hit counter
                            executionTime += l2Cache->hitTime;

                        } else { // Miss in L2 Cache, read from memory
                            if (debug) {
                                printf("\t MISS: Not found in L2, adding L2 miss time (+%d)\n", l2_miss_time);
                            }

                            executionTime += l2Cache->missTime;

                            // Find LRU to write over in L2
                            writeOverTag = moveBlock(l2Cache, l2Tag, l2IndexField, 0); // Tag is now in L2

                            if (writeOverTag) { // LRU in L2 was dirty
                                if (debug) {
                                    printf("\t Kicking out dirty block in L2: Tag = %llx \n", writeOverTag);
                                    printf("\t Rewriting dirty block L2 -> main memory (+%d)\n", mainMemoryTime);
                                }
                                executionTime += mainMemoryTime; // Move dirty block from L2 -> memory
                            }

                            // Bring block into L2
                            if (debug) {
                                printf("\t Bringing block back to L2, adding main memory -> L2 time (+%d)\n", mainMemoryTime);
                                printf("\t Adding L2 replay time (+%d)\n", l2Cache->hitTime);
                            }
                            executionTime += mainMemoryTime; // Move dirty block from L2 -> memory
                            executionTime += l2Cache->hitTime;
                        }

                        // Bring into L1 cache
                        if (debug) {
                            printf("\t Writing block back to L1...");
                            printf("Adding L2->L1 transfer time (+%d)\n", l2_transfer_time * (dCache->blockSize / l2_bus_width));
                            printf("\t Adding L1 replay time (+%d)\n", l1_hit_time);
                        }

                        // Time penalty
                        executionTime += l2_transfer_time * (dCache->blockSize / l2_bus_width); // Move desired block from L1 -> L2
                        executionTime += dCache->hitTime;
                    }
                } else if (opCode == 'W') {
                    if (!counted) {
                        writeCounter++; // Increment write counter
                        counted = 1;
                    }

                    // Track which indexes have been accessed
                    dCacheIndexTracker[l1IndexField] = 1;
                    l2CacheIndexTracker[l2IndexField] = 1;

                    hit = scanCache(dCache, l1Tag, l1IndexField, opCode); // Check L1I cache
                    if (hit == 1) { // Found in L1I cache
                        if (debug)
                            printf("\t HIT: Found in L1, adding L1 hit time (+%d)\n", l1_hit_time);
                        l1HitCounter++; //Increment hit counter
                        executionTime += dCache->hitTime;
                        // 
                    } else { // Not in L1 Cache, all tags were valid, LRU is dirty
                        if (debug) {
                            printf("\t MISS: Not found in L1, adding L1 miss time (+%d)\n", l1_miss_time);
                        }

                        l1MissCounter++;
                        executionTime += dCache->missTime;

                        // Find LRU to write over in L1
                        writeOverTag = moveBlock(dCache, l1Tag, l1IndexField, 1); // Tag is now in L1, do accounting details

                        if (writeOverTag) {
                            if (debug) printf("\t Need to write over a dirty block\n");
                            writeBack(writeOverTag); // Reconstruct fields for L2 write-back

                            if (debug) printf("\t L2 Writeback Index = %llx, Tag = %llx\n", l2WritebackIndex, l2WritebackTag);

                            if (scanCache(l2Cache, l2WritebackTag, l2WritebackIndex, 'W')) { // Be seen as a write
                                if (debug) {
                                    printf("\t\t HIT: Found in L2, adding L2 hit time\n");
                                    printf("Adding L1->L2 transfer time (+%d)\n", l2_transfer_time * (iCache->blockSize / l2_bus_width));
                                }
                                executionTime += l2Cache->hitTime;
                                executionTime += l2_transfer_time * (dCache->blockSize / l2_bus_width); // Move desired block from L1 -> L2
                            } else {
                                if (debug) printf("\t\t MISS: Not Found in L2, adding L2 miss time\n");
                                executionTime += l2Cache->missTime;

                                // Find LRU to write over in L2
                                writeOverTag = moveBlock(l2Cache, l2WritebackTag, l2WritebackIndex, 1); // Mark as dirty

                                if (writeOverTag) {
                                    if (debug) {
                                        printf("\t\t Need to write over a dirty block\n");
                                        printf("\t\t Adding L2-> memory time (+%d)\n", mainMemoryTime);
                                    }
                                    executionTime += mainMemoryTime;
                                }

                                if (debug) printf("Bringing block into L2, adding memory -> L2 time (+%d)\n", mainMemoryTime);
                                executionTime += mainMemoryTime;

                                if (debug) printf("Adding L2 replay time (+%d)\n", l2Cache->hitTime);
                                executionTime += l2Cache->hitTime;

                                if (debug) printf("Adding L1->L2 transfer time (+%d)\n", l2_transfer_time * (dCache->blockSize / l2_bus_width));
                                executionTime += l2_transfer_time * (iCache->blockSize / l2_bus_width); // Move desired block from L1 -> L2
                            }
                        }

                        // Scan L2
                        if (debug) printf("Now scanning L2 for target tag\n"
                                "\t *** L2: Index Field = %llx, Tag Field = %llx *** \n", l2IndexField, l2Tag);
                        if (scanCache(l2Cache, l2Tag, l2IndexField, opCode)) { // Hit in L2
                            if (debug)
                                printf("\t HIT: Found in L2, adding L2 hit time (+%d)\n", l2_hit_time);
                            l2HitCounter++; // Increment hit counter
                            executionTime += l2Cache->hitTime;

                        } else { // Miss in L2 Cache, read from memory
                            if (debug) {
                                printf("\t MISS: Not found in L2, adding L2 miss time (+%d)\n", l2_miss_time);
                            }

                            executionTime += l2Cache->missTime;

                            // Find LRU to write over in L2
                            writeOverTag = moveBlock(l2Cache, l2Tag, l2IndexField, 0); // Tag is now in L2

                            if (writeOverTag) { // LRU in L2 was dirty
                                if (debug) {
                                    printf("\t Kicking out dirty block in L2: Tag = %llx \n", writeOverTag);
                                    printf("\t Rewriting dirty block L2 -> main memory (+%d)\n", mainMemoryTime);
                                }
                                executionTime += mainMemoryTime; // Move dirty block from L2 -> memory
                            }

                            // Bring block into L2
                            if (debug) {
                                printf("\t Bringing block back to L2, adding main memory -> L2 time (+%d)\n", mainMemoryTime);
                                printf("\t Adding L2 replay time (+%d)\n", l2Cache->hitTime);
                            }
                            executionTime += mainMemoryTime; // Move dirty block from L2 -> memory
                            executionTime += l2Cache->hitTime;
                        }

                        // Bring into L1 cache
                        if (debug) {
                            printf("\t Writing block back to L1...");
                            printf("Adding L2->L1 transfer time (+%d)\n", l2_transfer_time * (dCache->blockSize / l2_bus_width));
                            printf("\t Adding L1 replay time (+%d)\n", l1_hit_time);
                        }

                        // Time penalty
                        executionTime += l2_transfer_time * (dCache->blockSize / l2_bus_width); // Move desired block from L1 -> L2
                        executionTime += dCache->hitTime;
                    }
                } // Finished reading trace

                // Sent 4 bytes to the processor

                int transferred = 0;
                //                moved = 0;
                // Find the next fields if you need to move on to the next block
                while ((byteStartAddress < byteEndAddress) && (bytesLeft > 0)) {
                    if (byteStartAddress >= address) { // Moved desired bytes
                        //                        byteAddress++;
                        transferred++;
                        bytesLeft--;
                    }
                    byteStartAddress++;
                    //                    moved++;
                }

                if (debug) printf("%d bytes transferred.\n", transferred);

                //                if (transferred == 4 && traceCounter == 25) {
                //                    printCacheStatus(dCache, dCacheIndexTracker);
                //                    printCacheStatus(l2Cache, l2CacheIndexTracker);
                //                }

                if (bytesLeft > 0) { // Reached end of cache block
                    misalignments++;    // Have to do multiple memory access
                    if (byteEndAddress != cacheEndAddress) { // We can go on to another word in the same block
                        //                        byteStartAddress++;
                        byteEndAddress = byteStartAddress + 4;
                        if (debug) {
                            printf("%d bytes left. Still in the same block.\n\n", bytesLeft);
                            printf("\t *** Byte Start/End Address = %llx, \t%llx *** \n", byteStartAddress, byteEndAddress);
                        }
                    } else {
                        // Move on to the next block
                        //                        byteStartAddress; // Potential overflow error HERE
                        byteEndAddress = byteStartAddress + 4;
                        findFields();

                        if (debug) {
                            printf("\t Need to access the next block! \n");
                            printf("\t Requested Cache Block Start/End address = %llx, \t%llx\n", cacheStartAddress, cacheEndAddress);
                            printf("\t *** Byte Start/End Address = %llx, \t%llx *** \n", byteStartAddress, byteEndAddress);
                            printf("\t *** L1: Index Field = %llx, Tag Field = %llx *** \n\n", l1IndexField, l1Tag);
                        }
                    }
                } else {
                    if (debug) printf("%d bytes left.\n", bytesLeft);
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

            traceTime = executionTime - traceTime;
            if (debug) {
                printf("\t ---> Total trace time: %d\n", traceTime);
                printf("\t ---> Total execution time: %llu\n", executionTime);
                printf("-------------------- End of Trace --------------------\n");
            }

            switch (opCode) {
                    //                printf("TraceTime: %d", traceTime);
                case 'R':
                    readTime += traceTime;
                    if (debug) printf("ReadTime= %llu\n", readTime);
                    break;
                case 'W':
                    writeTime += traceTime;
                    if (debug) printf("WriteTime= %llu\n", writeTime);
                    break;
                case 'I':
                    instructionTime += traceTime;
                    if (debug) printf("InstTime= %llu\n", instructionTime);
                    break;
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

//    printf("---------------------------------------------------\n");
//    printf("Cache Final Contents: \n\n"
//            "Memory Level: L1 Instruction Cache\n");
//    printCacheStatus(iCache, iCacheIndexTracker);
//
//    printf("\nMemory Level: L1 Data Cache\n");
//    printCacheStatus(dCache, dCacheIndexTracker);
//
//    printf("\nMemory Level: L2 Unified Cache \n");
//    printCacheStatus(l2Cache, l2CacheIndexTracker);

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
            result = 50*(size/65536); // L2 cache memory costs $50 per 64KB
        // Add an additional $50 for each doubling in associativity
        if (associativity > 1)
            result += 50 * log(associativity)/ log(2);
        else
            result+= 50;
    }
    return result;
}

void findFields() {
    // Pull out the byte field from the address
    // Fill space with 1's, then shift over by byeOffset
    l1Byte = (~0) << iCache->byteOffsetSize;
    l2Byte = (~0) << l2Cache->byteOffsetSize;

    cacheStartAddress = l1Byte;
    cacheStartAddress = cacheStartAddress & byteStartAddress; // Track start of the block
    cacheEndAddress = cacheStartAddress + iCache->blockSize; // Track end of the block

    l1Byte = ~l1Byte; // Switch all 1's to 0's
    l1Byte = l1Byte & byteStartAddress; // Mask out all fields but the incoming byte field

    l2Byte = ~l2Byte; // Switch all 1's to 0's
    l2Byte = l2Byte & byteStartAddress; // Mask out all fields but the incoming byte field

    //    desiredByteAddress = startAddress; // Keep track of start of desired bytes

    // Pull out the index field from the address
    // Fill space with 1's, then shift over by byeOffset
    l1IndexField = (~0) << (iCache->indexFieldSize + iCache->byteOffsetSize);
    l2IndexField = (~0) << (l2Cache->indexFieldSize + l2Cache->byteOffsetSize);
    l1Tag = l1IndexField; // Save correct placing of 1's for finding the tag field next
    l1IndexField = ~l1IndexField; // Switch all 1's to 0's
    l1IndexField = l1IndexField & byteStartAddress; // Mask out all fields but the incoming byte/index field

    l2Tag = l2IndexField; // Save correct placing of 1's for finding the tag field next
    l2IndexField = ~l2IndexField; // Switch all 1's to 0's
    l2IndexField = l2IndexField & byteStartAddress; // Mask out all fields but the incoming byte/index field

    // Shift out the byte field
    l1IndexField = l1IndexField >> iCache->byteOffsetSize;
    l2IndexField = l2IndexField >> l2Cache->byteOffsetSize;

    // Pull out the tag field
    l1Tag = l1Tag & byteStartAddress;
    l2Tag = l2Tag & byteStartAddress;

    // Shift out other fields
    l1Tag = l1Tag >> (iCache->indexFieldSize + iCache->byteOffsetSize);
    l2Tag = l2Tag >> (l2Cache->indexFieldSize + l2Cache->byteOffsetSize);

    return;
}

//void findNextFields() {
//    // Pull out the byte field from the address
//    // Fill space with 1's, then shift over by byeOffset
//    l1Byte = (~0) << iCache->byteOffsetSize;
//    l2Byte = (~0) << l2Cache->byteOffsetSize;
//
//    l1Byte = ~l1Byte; // Switch all 1's to 0's
//    l1Byte = l1Byte & byteAddress; // Mask out all fields but the incoming byte field
//
//    l2Byte = ~l2Byte; // Switch all 1's to 0's
//    l2Byte = l2Byte & byteAddress; // Mask out all fields but the incoming byte field
//
//    endAddress = l1Byte;
//    endAddress = endAddress & address;
//    endAddress = endAddress + iCache->blockSize; // Track end of the block
//
//    // Pull out the index field from the address
//    // Fill space with 1's, then shift over by byeOffset
//    l1IndexField = (~0) << (iCache->indexFieldSize + iCache->byteOffsetSize);
//    l2IndexField = (~0) << (l2Cache->indexFieldSize + l2Cache->byteOffsetSize);
//    l1Tag = l1IndexField; // Save correct placing of 1's for finding the tag field next
//    l1IndexField = ~l1IndexField; // Switch all 1's to 0's
//    l1IndexField = l1IndexField & byteAddress; // Mask out all fields but the incoming byte/index field
//
//    l2Tag = l2IndexField; // Save correct placing of 1's for finding the tag field next
//    l2IndexField = ~l2IndexField; // Switch all 1's to 0's
//    l2IndexField = l2IndexField & byteAddress; // Mask out all fields but the incoming byte/index field
//
//    // Shift out the byte field
//    l1IndexField = l1IndexField >> iCache->byteOffsetSize;
//    l2IndexField = l2IndexField >> l2Cache->byteOffsetSize;
//
//    // Pull out the tag field
//    l1Tag = l1Tag & byteAddress;
//    l2Tag = l2Tag & byteAddress;
//
//    // Shift out other fields
//    l1Tag = l1Tag >> (iCache->indexFieldSize + iCache->byteOffsetSize);
//    l2Tag = l2Tag >> (l2Cache->indexFieldSize + l2Cache->byteOffsetSize);
//
//    return;
//}

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
    
    flushTime = iCache->flushTime + dCache->flushTime + l2Cache->flushTime;
    printf("Flush time = %llu\n", flushTime);
    printf("Total time (Execution Time + Flush Time) = %llu\n\n", flushTime + executionTime);

    printf("Number of Reference Types: \t[Percentage] \n"
            "\tReads \t= \t%llu \t\t[%.1f%%]\n"
            "\tWrites \t= \t%llu  \t\t[%.1f%%]\n"
            "\tInst. \t= \t%llu \t\t[%.1f%%]\n"
            "\tTotal \t= \t%llu\n\n",
            readCounter, (double) readCounter * 100 / traceCounter,
            writeCounter, (double) writeCounter * 100 / traceCounter,
            instructionCounter, (double) instructionCounter * 100 / traceCounter,
            readCounter + writeCounter + instructionCounter);

    printf("Total Cycles for Activities: \t[Percentage] \n"
            "\tReads \t= \t%llu \t\t[%.1f%%]\n"
            "\tWrites \t= \t%llu \t\t[%.1f%%]\n"
            "\tInst. \t= \t%llu \t\t[%.1f%%]\n"
            "\tTotal \t= \t%llu\n\n",
            readTime, (double) readTime * 100 / executionTime,
            writeTime, (double) writeTime * 100 / executionTime,
            instructionTime, (double) instructionTime * 100 / executionTime,
            readTime + writeTime + instructionTime);

    double readCycles = 0, writeCycles = 0, insCycles = 0;
    if (readCounter)
        readCycles = ((double) readTime) / ((double) readCounter);
    if (writeCounter)
        writeCycles = ((double) writeTime) / ((double) writeCounter);
    if (instructionCounter)
        insCycles = ((double) instructionTime) / ((double) instructionCounter);
    
//    printf("instructionCounter = %llu", instructionCounter);
//    printf("instructionTime = %llu", instructionTime);
//    printf("Should be = %f", insCycles);
    

    printf("Average Cycles for Activities: \n"
            "\tReads = %.1f, \tWrites = %.1f, \tInstructions = %.1f\n\n",
            readCycles, writeCycles, insCycles);

    unsigned long long idealTime = readCounter + writeCounter + (2 * instructionCounter);

    printf("Ideal: Execution Time = %llu; \tCPI = %.1f\n", idealTime, ((double) idealTime) / ((double) instructionCounter));
    printf("Ideal Misaligned Execution Time = %llu; \tCPI = %.1f\n\n", idealTime + misalignments,((double) idealTime + (double)misalignments) / ((double) instructionCounter) );

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

void writeBack(unsigned long long writeBackTag) {
    unsigned long long addressTemp = 0; // Fill with 1s
    addressTemp += (l1IndexField << iCache->byteOffsetSize); // Put index field back in the right spot
    addressTemp += (writeBackTag << (iCache->byteOffsetSize + iCache->indexFieldSize)); // Put tag field back in the right spot

    // Pull out L2 Writeback index/tag
    l2WritebackIndex = ~0; // Fill with ones
    l2WritebackIndex = l2WritebackIndex << (l2Cache->byteOffsetSize + l2Cache->indexFieldSize);

    l2WritebackTag = l2WritebackIndex; // 1's only in the tag field

    l2WritebackIndex = ~l2WritebackIndex; // 1's only in the byte/index field

    l2WritebackIndex = l2WritebackIndex & addressTemp; // Pulled out index
    l2WritebackTag = l2WritebackTag & addressTemp; // Pulled out index

    l2WritebackIndex = l2WritebackIndex >> l2Cache->byteOffsetSize; // shift over
    l2WritebackTag = l2WritebackTag >> (l2Cache->byteOffsetSize + l2Cache->indexFieldSize);

    return;
}

void findIdealTimes() {

}
