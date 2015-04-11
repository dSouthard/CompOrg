#include "cache.h"


#include<stdlib.h>
#include<math.h>
#include<stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>


/*********************** Initialization ***********************/
struct Cache * initialize(int newCacheSize, int newBlockSize, int newAssociativity, int newMissTime, int newHitTime) {
  struct Cache *cache;
  struct Block *temp;
  int i, j;	// used for iteration

  // Create in memory
  cache=(struct Cache*)malloc(sizeof(struct Cache));

  // Initalize cache variable fields based on config files
  cache->cacheSize=newCacheSize;
  cache->blockSize=newBlockSize;
  cache->associativity=newAssociativity;
  cache->lengthOfWay=(newCacheSize/newBlockSize)/newAssociativity;
  cache->indexFieldSize=log(cache->lengthOfWay)/log(2);	// log(base2)(lengthOfWay)
  cache->byteOffsetSize=log(newBlockSize)/log(2);	// log(base2)(newBlockSize)
  cache->missTime=newMissTime;
  cache->hitTime=newHitTime;

  // Initialize tracking variables to 0
  cache->hits=0;
  cache->misses=0;
  cache->writeRefs=0;
  cache->readRefs=0;
  cache->instructionTime=0;
  cache->readTime=0;
  cache->writeTime=0;
  cache->dirtyKickouts=0;
  cache->kickouts=0;
  
  // Initialize array of blocks
  cache->blockArray=(struct Block **)malloc((cache->lengthOfWay*sizeof(struct Block *)));
  for (i=0;i<cache->lengthOfWay;i++) {
    // First column in blockArray will be a dummy pointer => add an extra "way"(column)
    // Go through each row and allocate space for necessary amount of blocks
    cache->blockArray[i]=(struct Block *)malloc((newAssociativity+1)*sizeof(struct Block));

    // Go through and intialize blocks
    for (j=0;j < newAssociativity;j++) {
    // Set valid, dirty, and tag fields
	cache->blockArray[i][j].valid = 0;
	cache->blockArray[i][j].dirty = 0;
	cache->blockArray[i][j].tag = 0;

    // Point to next block in LRU chain
	cache->blockArray[i][j].nextBlock = ache->blockArray[i][j+1];
     }
     
    // Point last block to null
    cache->blockArray[i][newAssociativity].nextBlock = null;
  }

  // Done
  return cache;
}

/********************* Read Trace Function ***************************/
unsigned long long readTrace(struct Cache* cache,unsigned long long  t, unsigned long long index, char op){
    int i, j;
    struct Block *t1, *t2;
    for (j=0;j<cache->ways;j++) {
      if (cache->bloarr[index][j].tag==t) {
        if (!cache->bloarr[index][j].valid) {
          cache->bloarr[index][j].tag=t;
          cache->bloarr[index][j].valid=1;
          return 0;
        }
        if (op=='W') {
          cache->bloarr[index][j].dirty=1;
        }
        cache->hits++;
        if (&cache->bloarr[index][j]==cache->lruarray[index]) {
          return 0;
        }
        t1=cache->lruarray[index];
        while (t1!=&cache->bloarr[index][j]) {
          t1 = t1->lru;
        }
        t2=cache->lruarray[index];
        while ((t2->lru)!=&cache->bloarr[index][j]) {
          t2 = t2->lru;
        }
        if (t1->lru!=NULL) {
          t2->lru=t1->lru;
        }
        else {
          t2->lru=NULL;
        }
        t1->lru=cache->lruarray[index];
        cache->lruarray[index]=t1;

        return 0;
      }
    }
    t1=cache->lruarray[index];

    while (t1->lru!=NULL) {
      t1=t1->lru;
    }
    if (t1->valid==1) {
      cache->ko++;
    }

    if (t1->dirty==1) {
      cache->dko++;
      t1->dirty=0;
      t1->valid=0;

      return t1->tag;
    }


    t1=cache->lruarray[index];

    while (t1->lru!=NULL) {
      t1=t1->lru;
    }

    t1->tag=t;
    t1->valid=1;
    if (op=='W') {
      if (t1->valid) {
        t1->dirty=1;
      }
      else {
        t1->dirty=0;
      }
    }

    t1->lru=cache->lruarray[index];
    cache->lruarray[index]=t1;


    while (t1->lru!=cache->lruarray[index]) {
      t1=t1->lru;
    }
    t1->lru=NULL;
    cache->misses++;


    return 1;
}

/*************************** Print Functions ***********************/
void printcache(struct Cache *cache) {
  int i, j;
  for (j=0;j<cache->setlength;j++) {
    printf("----------------------------------------------------\n");
    for (i=0;i<cache->ways;i++) {
      if (cache->bloarr[j][i].tag) {
        printf("| V: %d D: %d T: %llX  |", cache->bloarr[j][i].valid, cache->bloarr[j][i].dirty, cache->bloarr[j][i].tag);
      }
      else {
        printf("|           |");
      }
    }
    printf("\n");
  }
  printf("-----------------------------------------------------\n");
}

int calcost(int level, int size, int associativity) {
  double result;
  int i;
  if (level==1) {
    result = 100*(size/4096);
    for (i=0;i<size/4096;i++) {
      result += 100*log(associativity)/log(2);
    }
  }
  if (level==2) {
    if (size%65536) {
      size+=(65536-size);
    }
    result = 50*(size/65536);
    result += 50*log(associativity)/log(2);
  }

  return (int)result;
}

void printstuff(struct Cache *icache, struct Cache* dcache, struct Cache* l2cache, int mready, int mchunks, int mchunkt, unsigned long long refnum, unsigned long long misallignment, int memcost){
  unsigned long long extime;
  struct winsize w;
  int width, i;

  extime=icache->itime+dcache->rtime+dcache->wtime+l2cache->itime+l2cache->rtime+l2cache->wtime;

  ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
  width=w.ws_col;
  for(i=0;i<width;i++) {
    printf("-");
  }
  printf("\n");

  for(i=0;i<(width/2-9);i++) {
    printf(" ");
  }
  printf("Simulation Results");
  for(i=0;i<(width/2-9);i++) {
    printf(" ");
  }
  printf("\n");

  for(i=0;i<width;i++) {
    printf("-");
  }
  printf("\n\n");

  printf("Memory System:\n");
  printf("    Dcache Size = %d : ways = %d : block size = %d\n", dcache->cachesize,dcache->ways,dcache->blocksize);
  printf("    Icache Size = %d : ways = %d : block size = %d\n", icache->cachesize,icache->ways,icache->blocksize);
  printf("    L2-cache Size = %d : ways = %d : block size = %d\n", l2cache->cachesize, l2cache->ways,l2cache->blocksize);
  printf("    Memory ready  time = %d : chunksize = %d : chunktime = %d\n\n", mready,mchunks,mchunkt);
  printf("Execute Time = %lld; Total References = %lld\n", extime, refnum+1);
  printf("Instruction Refereneces = %lld; Data refs = %lld\n",icache->rrefs,dcache->wrefs+dcache->rrefs);
  printf("Number of references types: [Percentage]\n");
  printf("    Reads  =     %lld    [%f%%]\n",dcache->rrefs,100*((double)dcache->rrefs)/(dcache->rrefs+dcache->wrefs+icache->rrefs));
  printf("    Writes =     %lld    [%f%%]\n",dcache->wrefs,100*((double)dcache->wrefs)/(dcache->rrefs+dcache->wrefs+icache->rrefs));
  printf("    Inst.  =     %lld    [%f%%]\n",icache->rrefs,100*((double)icache->rrefs)/(dcache->rrefs+dcache->wrefs+icache->rrefs));
  printf("    Total  =     %lld\n",dcache->rrefs+dcache->wrefs+icache->rrefs);
  printf("Total cycles for activities: [Percentage]\n");
  printf("    Reads  =     %lld    [%f%%]\n",dcache->rtime+l2cache->rtime,100*((double)(dcache->rtime+l2cache->rtime))/extime);
  printf("    Writes =     %lld    [%f%%]\n",dcache->wtime+l2cache->wtime,100*((double)(dcache->wtime+l2cache->wtime))/extime);
  printf("    Inst.  =     %lld    [%f%%]\n",icache->itime+l2cache->itime,100*((double)(icache->itime+l2cache->itime))/extime);
  printf("    Total  =     %lld\n",extime);
  printf("Average cycles for activiites: [Percentage]\n");
  printf("  Read =  %f; Write =  %f; Inst. =  %f\n",((double)dcache->rtime+l2cache->rtime)/(dcache->rrefs),((double)dcache->wtime+l2cache->wtime)/(dcache->wrefs),((double)extime)/(icache->rrefs));
  printf("Ideal: Exec. Time = %lld; CPI =  %f\n",dcache->rrefs+dcache->wrefs+2*icache->rrefs,((double)dcache->rrefs+dcache->wrefs+2*icache->rrefs)/(icache->rrefs));
  printf("Ideal mis-aligned: Exec. Time = %lld; CPI =  %f\n\n",dcache->rrefs+dcache->wrefs+2*icache->rrefs+misallignment,((double)dcache->rrefs+dcache->wrefs+2*icache->rrefs+misallignment)/(icache->rrefs));
  printf("Memory Level: L1i\n");
  printf("    Hit Count = %lld  Miss Count = %lld\n",icache->hits,icache->misses);
  printf("    Total Requests = %lld\n",icache->hits+icache->misses);
  printf("    Hit Rate = %f%%   Miss Rate = %f%%\n",100*((float)(icache->hits)/(icache->hits+icache->misses)),100*((float)(icache->misses)/(icache->misses+icache->hits)));
  printf("    Kickouts = %lld; Dirty Kickouts = %lld; Transfers = %lld\n",icache->ko,icache->dko,icache->misses);
  printf("\n");
  printf("Memory Level: L1d\n");
  printf("    Hit Count = %lld  Miss Count = %lld\n",dcache->hits,dcache->misses);
  printf("    Total Requests = %lld\n",dcache->hits+dcache->misses);
  printf("    Hit Rate = %f%%   Miss Rate = %f%%\n",100*((float)(dcache->hits)/(dcache->hits+dcache->misses)),100*((float)(dcache->misses)/(dcache->misses+dcache->hits)));
  printf("    Kickouts = %lld; Dirty Kickouts = %lld; Transfers = %lld\n",dcache->ko,dcache->dko,dcache->misses);
  printf("\n");
  printf("Memory Level: L2\n");
  printf("    Hit Count = %lld  Miss Count = %lld\n",l2cache->hits,l2cache->misses);
  printf("    Total Requests = %lld\n",l2cache->hits+l2cache->misses);
  printf("    Hit Rate = %f%%   Miss Rate = %f%%\n",100*((float)(l2cache->hits)/(l2cache->hits+l2cache->misses)),100*((float)(l2cache->misses)/(l2cache->misses+l2cache->hits)));
  printf("    Kickouts = %lld; Dirty Kickouts = %lld; Transfers = %lld\n",l2cache->ko,l2cache->dko,l2cache->misses);
  printf("\n");
  printf("L1 cache cost (Icache $%d) + (Dcache $%d) = $%d\n", icache->cost, dcache->cost, icache->cost+dcache->cost);
  printf("L2 cache cost = $%d;   Memory Cost = $%d\n", l2cache->cost, memcost);
  printf("Total cost = $%d\n\n", memcost+icache->cost+dcache->cost+l2cache->cost);

  for(i=0;i<width;i++) {
    printf("-");
  }
  printf("\n");
}


