#include <stdio.h>
#include <getopt.h>
#include <math.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>
#include "cache.h"


// Global Variables
// Cache structures
struct Cache *icache, *dcache, *l2cache; // L1 I and D caches, L2 Unified cache

// Main memory parameters, taken from project description
int mem_sendaddr = 10;
int mem_ready = 30;
int mem_chunktime = 15;
int mem_chunksize = 8;

// Calculate cost of the system
double l1Cost=0, l2Cost = 0, mainMemoryCost = 0;

void printHelpStatement() {
    char [] helpStatement = "Memory Simulation Project, ECEN 4593 \n"
            + "Usage: cat <traces> | ./cachesim <config_file> -FLAGS\n"
            + "Configuration File: Used to set up caches. \n\n"
            + "Flag Options:\n"
            + "\t -h: Print up usage statement\n"
            + "\t -v: Turn on debugging statements";
    printf(helpStatement);
}

void printSetup() {
    printf("Memory System:\n");
    printf("\t L1 Data Cache: Size = %d, Associativity = %d : Block size = %d\n", dcache->cacheSize, dcache->associativity, dcache->blockSize);
    printf("\t L1 Instruction Cache: Size = %d, Associativity = %d : Block size = %d\n", icache->cacheSize, icache->associativity, icache->blockSize);
    printf("\t L1 Cache Costs: %.2lf each, %.2lf total/n", l1Cost, l1Cost*2);
    printf("\t L2 Unified Cache: Size = %d, Associativity = %d : Block size = %d\n", l2cache->cacheSize, l2cache->associativity, l2cache->blockSize);
    printf("\t L2 Cache Costs: %.2lf \n", l2Cost);
    printf("\t Main Memory Parameters: \n"
            + "\t\t Time to send the address to memory = %d,\n"
            + "\t\t Time for the memory to be ready for start of transfer = %d,\n"
            + "\t\t Time to send/receive a single bus-width of data = %d,\n"
            + "\t\t Width of the bus interface to memory (bytes) = %d,\n",
            mem_sendaddr, mem_ready, mem_chunktime, mem_chunksize);
    printf("\t Main Memory Costs: %.2lf \n", mainMemoryCost);


}

unsigned long long updateaddr(struct Cache *cache, unsigned long long addrin, int byte) {
    unsigned long long addr;


    addr = addrin + cache->blocksize;

    return addr;
}

unsigned long long reconstruct(struct Cache *cache, unsigned long long tag, unsigned long long index) {
    unsigned long long result;
    result = (tag << (cache->bytesize + cache->indexsize)) | (index << cache->bytesize);
    return result;
}

int refs(int byte, int size, int blocksize) {
    int len;
    if ((size + (byte % blocksize)) % blocksize) {
        len = ((size + (byte % blocksize)) / blocksize) + 1;
    } else {
        len = ((size + (byte % blocksize)) / blocksize);
    }
    return len;
}

int main(int argc, char* argv[]) {
    //**************************  Local Variables
    // Variables to read in traces
    char opCode; // Track type of trace: R [Read], W[Write], or I[Instruction]
    unsigned long long traceNumber = -1; // Track number of traces received
    unsigned long long address[10] = 0; // Read in new addresses
    int size = 0; // Track number of bytes referenced by trace
    FILE *configFile; // File to read in configuration file

    // Variables for adjusting trace fields
    unsigned long long res[10];
    unsigned long long tag[10], index[10], byte, l2tag[10], l2index[10], wbaddr, wbtag, wbindex;
    int verbose = 0;

    // Variables to calculate cost of accessing memory
    int l2transtime = 6, l2buswidth = 16; // L1 -> L2 access penalties
    unsigned long long totaltime = 0; // Track total execution time
    unsigned long long misallignments = 0; // Track total misalignments

    // Default cache values
    // Use the fact that L1 Data and Instruction caches are always the same size
    int l1size = 8192, l2size = 32768, l1assoc = 1, l2assoc = 1;

    // Check input arguments
    if (argc < 2 || argc > 4) {
        // Didn't input Configuration file or put in too many arguments, print help statement
        printHelpStatement();
        return -1;
    }

    if (argc > 2) { // Flags were added
        int i = 2;
        for (i = 2; i < argc; i++) {
            if (argv[i] == "-h") // Print help statement
                printHelpStatement();
            if (argv[i] == "-v") // Start debugging statements
                verbose = 1;
        }
    }

    // Read in configFile
    configFile = fopen(argv[1], "r");

    int parameter = 0;
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
            mem_chunksize = val;
        }
    }

    // Initiate caches based on configFile values
    icache = initcache(l1size, 32, l1assoc, 1, 1);
    dcache = initcache(l1size, 32, l1assoc, 1, 1);
    l2cache = initcache(l2size, 64, l2assoc, 8, 5);

    // Calculate cost of the system based on configuration sizes
    l1Cost = calculateCost(1, l1size, l1assoc);
    l2Cost = calculateCost(2, l2size, l2assoc);
    int memChunkDivider;
    if (mem_chunksize <= 16)
        memChunkDivider = 1;
    else
        memChunkDivider = mem_chunksize/16;
    mainMemoryCost = 50 + (memChunkDivider * 25);

    printSetup();

    
    // ***************************** Start Reading in the Traces
    icache->misses = 0;
    while (scanf("%c %llX %d\n", &op, &addr[0], &size) == 3) {
        if (verbose) {
            printf("%c,%llX,%d\n", op, addr[0], size);
        }
        refnum++;
        begin = icache->itime + dcache->rtime + dcache->wtime + l2cache->rtime + l2cache->wtime + l2cache->itime;

        if (op == 'I') {
            icache->rrefs++;
            byte = (~((~0) << icache->bytesize)) & addr[0];
            length = refs(byte, size, icache->blocksize);
            addr[0] = addr[0]&(~3);
            misallignments += length - 1;
            if (verbose) {
                printf("Ref %lld: Addr = %llx, Type = %c, BSize = %d\n", refnum, addr[0], op, size);
            }
            for (i = 0; i < length; i++) {
                tag[i] = (((~0) << (icache->indexsize + icache->bytesize)) & addr[i])>>(icache->indexsize + icache->bytesize);
                index[i] = (((~((~0) << icache->indexsize)) << icache->bytesize) & addr[i])>>(icache->bytesize);
                if (verbose) {
                    printf("Level L1i access addr = %llx, offset = %llx, reftype = Read\n", addr[i], byte);
                    printf("    index = %llX, tag =   %llx  ", index[i], tag[i]);
                }
                res[i] = readd(icache, tag[i], index[i], op);
                if (res[i]) {
                    icache->itime += icache->misstime;
                } else {
                    icache->itime += icache->hittime;
                }

                if (verbose) {
                    if (res[i]) {
                        printf("MISS\n");
                        printf("Add L1i miss time (+ %d)\n", icache->misstime);
                    } else {
                        printf("HIT\n");
                        printf("Add L1i hit time (+ %d)\n", icache->hittime);
                    }
                }
                addr[i + 1] = updateaddr(icache, addr[i], byte);
                byte = 0;
                if (res[i]) {
                    l2tag[i] = (((~0) << (l2cache->indexsize + l2cache->bytesize)) & addr[i])>>(l2cache->indexsize + l2cache->bytesize);
                    l2index[i] = (((~((~0) << l2cache->indexsize)) << l2cache->bytesize) & addr[i])>>(l2cache->bytesize);
                    if (verbose) {
                        printf("Level L2 access addr = %llx, offset = %llx, reftype = Read\n", addr[i], byte);
                        printf("    index = %llX, tag =   %llx  ", l2index[i], l2tag[i]);
                    }

                    if (res[i] > 1) {
                        wbaddr = reconstruct(icache, res[i], index[i]);
                        wbtag = (((~0) << (l2cache->indexsize + l2cache->bytesize)) & wbaddr)>>(l2cache->indexsize + l2cache->bytesize);
                        wbindex = (((~((~0) << l2cache->indexsize)) << l2cache->bytesize) & wbaddr)>>(l2cache->bytesize);
                        res[i] = readd(l2cache, wbtag, wbindex, 'W');
                        if (res[i] > 1) {
                            readd(l2cache, wbtag, wbindex, op);
                        }

                        res[i] = readd(icache, tag[i], index[i], op);
                        if (res[i]) {
                            readd(l2cache, l2tag[i], l2index[i], op);
                            if (res[i] > 1) {
                                readd(l2cache, l2tag[i], l2index[i], op);
                            }
                        }

                    } else {
                        res[i] = readd(l2cache, l2tag[i], l2index[i], op);
                        if (res[i] > 1) {
                            readd(l2cache, l2tag[i], l2index[i], op);
                        }
                    }

                    if (res[i]) {
                        l2cache->itime += l2cache->misstime + l2cache->hittime;
                        l2cache->itime += memsendaddr + memready + memchunktime * l2cache->blocksize * 8 / memchunksize;
                        icache->itime += l2transtime * (icache->blocksize * 8 / l2buswidth);
                        icache->itime += icache->hittime;
                    } else {
                        l2cache->itime += l2cache->hittime;
                        l2cache->itime += l2transtime * (icache->blocksize * 8 / l2buswidth) + icache->hittime;
                    }


                    if (verbose) {
                        if (res[i]) {
                            printf("MISS\n");
                            printf("Add L2 miss time (+ %d)\n", l2cache->misstime);
                            printf("Bringing line into L2.\n");
                            printf("Add memory to L2 transfer time (+ %d)\n", memsendaddr + memready + memchunktime * l2cache->blocksize * 8 / memchunksize);
                            printf("Add L2 hit replay time (+ %d)\n", l2cache->hittime);
                            printf("Bringing line into L1i.\n");
                            printf("Add L2 to L1 transfer time (+ %d)\n", l2transtime * (icache->blocksize * 8 / l2buswidth));
                            printf("Add L1i hit replay time (+ %d)\n", icache->hittime);
                        } else {
                            printf("HIT\n");
                            printf("Add L2 hit time (+ %d)\n", l2cache->hittime);
                            printf("Bringing line into L1i.\n");
                            printf("Add L2 to L1 transfer time (+ %d)\n", l2transtime * (icache->blocksize * 8 / l2buswidth));
                            printf("Add L1i hit replay time (+ %d)\n", icache->hittime);
                        }
                    }
                }
            }
        }
        if (op == 'R') {
            dcache->rrefs++;
            byte = (~((~0) << dcache->bytesize)) & addr[0];
            length = refs(byte, size, dcache->blocksize);
            misallignments += length - 1;
            addr[0] = addr[0]&(~3);
            if (verbose) {
                printf("Ref %lld: Addr = %llx, Type = %c, BSize = %d\n", refnum, addr[0], op, size);
            }
            for (i = 0; i < length; i++) {
                tag[i] = (((~0) << (dcache->indexsize + dcache->bytesize)) & addr[i])>>(dcache->indexsize + dcache->bytesize);
                index[i] = (((~((~0) << dcache->indexsize)) << dcache->bytesize) & addr[i])>>(dcache->bytesize);
                if (verbose) {
                    printf("Level L1d access addr = %llx, offset = %llx, reftype = Read\n", addr[i], byte);
                    printf("    index = %llX, tag =   %llx  ", index[i], tag[i]);
                }
                res[i] = readd(dcache, tag[i], index[i], op);
                if (res[i]) {
                    dcache->rtime += dcache->misstime;
                } else {
                    dcache->rtime += dcache->hittime;
                }

                if (verbose) {
                    if (res[i]) {
                        printf("MISS\n");
                    } else {
                        printf("HIT\n");
                    }
                }
                addr[i + 1] = updateaddr(dcache, addr[i], byte);
                byte = 0;
                if (res[i]) {
                    l2tag[i] = (((~0) << (l2cache->indexsize + l2cache->bytesize)) & addr[i])>>(l2cache->indexsize + l2cache->bytesize);
                    l2index[i] = (((~((~0) << l2cache->indexsize)) << l2cache->bytesize) & addr[i])>>(l2cache->bytesize);
                    if (verbose) {
                        printf("Level L2 access addr = %llx, offset = %llx, reftype = Read\n", addr[i], byte);
                        printf("    index = %llX, tag =   %llx  ", l2index[i], l2tag[i]);
                    }

                    if (res[i] > 1) {
                        wbaddr = reconstruct(dcache, res[i], index[i]);
                        wbtag = (((~0) << (l2cache->indexsize + l2cache->bytesize)) & wbaddr)>>(l2cache->indexsize + l2cache->bytesize);
                        wbindex = (((~((~0) << l2cache->indexsize)) << l2cache->bytesize) & wbaddr)>>(l2cache->bytesize);
                        res[i] = readd(l2cache, wbtag, wbindex, 'W');
                        if (res[i] > 1) {
                            readd(l2cache, wbtag, wbindex, op);
                        }

                        res[i] = readd(dcache, tag[i], index[i], op);
                        if (res[i]) {
                            readd(l2cache, l2tag[i], l2index[i], op);
                            if (res[i] > 1) {
                                readd(l2cache, l2tag[i], l2index[i], op);
                            }
                        }
                    } else {
                        res[i] = readd(l2cache, l2tag[i], l2index[i], op);
                        if (res[i] > 1) {
                            readd(l2cache, l2tag[i], l2index[i], op);
                        }

                    }
                    if (res[i]) {
                        l2cache->rtime += l2cache->misstime + l2cache->hittime;
                        l2cache->rtime += memsendaddr + memready + memchunktime * l2cache->blocksize * 8 / memchunksize;
                        dcache->rtime += l2transtime * (dcache->blocksize * 8 / l2buswidth);
                        dcache->rtime += dcache->hittime;
                    } else {
                        l2cache->rtime += l2cache->hittime;
                        dcache->rtime += l2transtime * (dcache->blocksize * 8 / l2buswidth) + dcache->hittime;
                    }


                    if (verbose) {
                        if (res[i]) {
                            printf("MISS\n");
                            printf("Add L2 miss time (+ %d)\n", l2cache->misstime);
                            printf("Bringing line into L2.\n");
                            printf("Add memory to L2 transfer time (+ %d)\n", memsendaddr + memready + memchunktime * l2cache->blocksize * 8 / memchunksize);
                            printf("Add L2 hit replay time (+ %d)\n", l2cache->hittime);
                            printf("Bringing line into L1i.\n");
                            printf("Add L2 to L1 transfer time (+ %d)\n", l2transtime * (dcache->blocksize * 8 / l2buswidth));
                            printf("Add L1d hit replay time (+ %d)\n", icache->hittime);
                        } else {
                            printf("HIT\n");
                            printf("Add L2 hit time (+ %d)\n", l2cache->hittime);
                            printf("Bringing line into L2.\n");
                            printf("Add L2 to L1 transfer time (+ %d)\n", l2transtime * (dcache->blocksize * 8 / l2buswidth));
                            printf("Add L1d hit replay time (+ %d)\n", dcache->hittime);
                        }
                    }
                }
            }
        }

        if (op == 'W') {
            dcache->wrefs++;
            byte = (~((~0) << dcache->bytesize)) & addr[0];
            length = refs(byte, size, dcache->blocksize);
            misallignments += length - 1;
            addr[0] = addr[0]&(~3);
            if (verbose) {
                printf("Ref %lld: Addr = %llx, Type = %c, BSize = %d\n", refnum, addr[0], op, size);
            }
            for (i = 0; i < length; i++) {
                tag[i] = (((~0) << (dcache->indexsize + icache->bytesize)) & addr[i])>>(dcache->indexsize + dcache->bytesize);
                index[i] = (((~((~0) << dcache->indexsize)) << dcache->bytesize) & addr[i])>>(dcache->bytesize);
                if (verbose) {
                    printf("Level L1d access addr = %llx, offset = %llx, reftype = Read\n", addr[i], byte);
                    printf("    index = %llX, tag =   %llx  ", index[i], tag[i]);
                }
                res[i] = readd(dcache, tag[i], index[i], op);
                if (res[i]) {
                    dcache->wtime += dcache->misstime;
                } else {
                    dcache->wtime += dcache->hittime;
                }

                if (verbose) {
                    if (res[i]) {
                        printf("MISS\n");
                        printf("Add L1d miss time (+ %d)\n", dcache->misstime);
                    } else {
                        printf("HIT\n");
                        printf("Add L1d hit time (+ %d)\n", dcache->hittime);
                    }
                }
                addr[i + 1] = updateaddr(dcache, addr[i], byte);
                byte = 0;
                if (res[i]) {
                    l2tag[i] = (((~0) << (l2cache->indexsize + l2cache->bytesize)) & addr[i])>>(l2cache->indexsize + l2cache->bytesize);
                    l2index[i] = (((~((~0) << l2cache->indexsize)) << l2cache->bytesize) & addr[i])>>(l2cache->bytesize);
                    if (verbose) {
                        printf("Level L2 access addr = %llx, offset = %llx, reftype = Read\n", addr[i], byte);
                        printf("    index = %llX, tag =   %llx  ", l2index[i], l2tag[i]);
                    }

                    if (res[i] > 1) {
                        wbaddr = reconstruct(dcache, res[i], index[i]);
                        wbtag = (((~0) << (l2cache->indexsize + l2cache->bytesize)) & wbaddr)>>(l2cache->indexsize + l2cache->bytesize);
                        wbindex = (((~((~0) << l2cache->indexsize)) << l2cache->bytesize) & wbaddr)>>(l2cache->bytesize);
                        res[i] = readd(l2cache, wbtag, wbindex, 'W');

                        if (res[i] > 1) {
                            readd(l2cache, wbtag, wbindex, op);
                        }

                        res[i] = readd(dcache, tag[i], index[i], op);
                        if (res[i]) {
                            readd(l2cache, l2tag[i], l2index[i], op);
                            if (res[i] > 1) {
                                readd(l2cache, l2tag[i], l2index[i], op);
                            }
                        }
                    } else {
                        res[i] = readd(l2cache, l2tag[i], l2index[i], op);
                        if (res[i] > 1) {
                            readd(l2cache, l2tag[i], l2index[i], op);
                        }

                    }

                    if (res[i]) {
                        l2cache->wtime += l2cache->misstime + l2cache->hittime;
                        l2cache->wtime += memsendaddr + memready + memchunktime * l2cache->blocksize * 8 / memchunksize;
                        dcache->wtime += l2transtime * (icache->blocksize * 8 / l2buswidth);
                        dcache->wtime += icache->hittime;
                    } else {
                        l2cache->wtime += l2cache->hittime;
                        dcache->wtime += l2transtime * (icache->blocksize * 8 / l2buswidth);
                        dcache->wtime += icache->hittime;
                    }

                    if (verbose) {
                        if (res[i]) {
                            printf("MISS\n");
                            printf("Add L2 miss time (+ %d)\n", l2cache->misstime);
                            printf("Bringing line into L2.\n");
                            printf("Add memory to L2 transfer time (+ %d)\n", memsendaddr + memready + memchunktime * l2cache->blocksize * 8 / memchunksize);
                            printf("Add L2 hit replay time (+ %d)\n", l2cache->hittime);
                            printf("Bringing line into L1i.\n");
                            printf("Add L2 to L1 transfer time (+ %d)\n", l2transtime * (dcache->blocksize * 8 / l2buswidth));
                            printf("Add L1d hit replay time (+ %d)\n", dcache->hittime);
                        } else {
                            printf("HIT\n");
                            printf("Add L2 hit time (+ %d)\n", l2cache->hittime);
                            printf("Bringing line into L2.\n");
                            printf("Bringing line into L1i.\n");
                            printf("Add L2 to L1 transfer time (+ %d)\n", l2transtime * (dcache->blocksize * 8 / l2buswidth));
                            printf("Add L1d hit replay time (+ %d)\n", dcache->hittime);
                        }
                    }
                }
            }
        }
        totaltime += icache->itime + dcache->rtime + dcache->wtime + l2cache->rtime + l2cache->wtime + l2cache->itime - begin;
        if (verbose) {
            printf("Simulated Time: %lld\n", totaltime);
        }
    }
    //if (verbose) {
    // printcache(icache);
    // printf("\n");
    // printcache(dcache);
    // printf("\n");
    //   printcache(l2cache);
    // }
    printstuff(icache, dcache, l2cache, memready, memchunksize, memchunktime, refnum, misallignments, memcost);
    return 0;
}

double calculateCost(int level, int size, int associativity) {
    double result;
    int i =0;
    if (level == 1) { // L1 Cache
        result = 100 * (size / 4096); // L1 cache memory costs $100 for each 4KB
        // Add an additional $100 for each doubling in associativity beyond direct-mapped
        for (i = 0; i < size / 4096; i++) {
            result += 100 * log(associativity) / log(2);
        }
    } 
    else if (level == 2) { // L2 Cache
        if (size < 65536) // Check if L2 memory is < 64Kb (still need to pay for full cost)
            size = 65536;
        result = 50 * (size / 65536); // L2 cache memory costs $50 per 64KB
        // Add additional $50 for each doubling in associativity
        result += 50 * log(associativity) / log(2);
    }
    return result;
}