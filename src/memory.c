/** @file memory.c
 *  @brief Implements starting point for a memory hierarchy with caching and RAM.
 *  @see memory.h
 */

#include "memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

static unsigned long instr_count;
/*L1 instruction cache*/
#define L1instr_CacheSize 32    //KB
#define L1instr_Associativity 4 //n-way
#define L1instr_BlockSize 64    //Bytes

/*L1 data cache*/
#define L1data_CacheSize 32     //KB
#define L1data_Associativity 4  //n-way
#define L1data_BlockSize 64     //Bytes

/*L2 unified cache*/
#define L2_CacheSize 256        //KB
#define L2_Associativity 4      //n-way
#define L2_BlockSize 64         //Bytes

typedef struct cache_array cache_array_t;
struct cache_array  //Structure for sets and blocks inside the cache
{
  unsigned int index; //locates which set inside the cache
  unsigned int tag;   //Locates the block inside the set
  unsigned int valid; //For checking if the block contains an adress
  unsigned int LRU;   //Replacement policy for the cache
  unsigned int dirtybit;
};

typedef struct cache cache_t;                                                       //Renaming "struct cache" to just "cache_t"
struct cache //struktur som inneholder parametre til cache
{
  cache_array_t **array;
  unsigned int size;
  unsigned int associativity;
  unsigned int blocksize;
  unsigned int buswidth;
  char *policy;
  unsigned int sets;
  unsigned int tag_bitsize;
  unsigned int index_bitsize;
  unsigned int offset_bitsize;
  unsigned int cachehit;
  unsigned int cachemiss;
  cache_t *next;
};

static cache_t *L1instructionCache, *L1dataCache, *L2unifiedCache;

//Creates a cache with given cache size(KB), associativity(n-way) and block size(bytes)
cache_t *cache_create(int size, int associativity, int blocksize)
{
  cache_t *cache = malloc(sizeof(cache_t));                                         //Allocate memory for the cache                                      //Setter av plass til cachen
  if(cache == NULL){                                                                //Hvis cachen er tom
    return NULL;                                                                    //Returner NULL
  }
  
  cache->cachehit = 0;
  cache->size = (size * 128);                                                         //Converting the size from kilobytes to bytes (from 32KB to 4096bytes)
  cache->blocksize = blocksize;
  cache->associativity = associativity;
  cache->offset_bitsize = log(cache->blocksize * 4) / log(2);                       //Calculating how many bits is needed for the block offset
  cache->sets = cache->size / (cache->blocksize * cache->associativity);            //Calculating how many sets the cache will contain
  cache->index_bitsize = log(cache->sets) / log(2);                                 //Calculating how many bits is needed for the index
  cache->tag_bitsize = (32 - cache->index_bitsize - cache->offset_bitsize);         //Calculating how many bits is needed for the tag
  //cache->buswidth = 64;
  //cache->policy = "LeastRecentlyUsed";
  //cache->cachemiss = 0;

  cache->array = calloc(cache->size, sizeof(cache_array_t));                        //Allocating memory for an empty array
  if(cache->array == NULL){
    if(cache != NULL){
      free(cache);
    }
    return NULL;
  }
 
  //For every struct/set in the array, the LRU value in set to 0,1,2 and 3
  int i = 0;
  
  for(i=0; i<cache->sets * cache->associativity; i++){
    cache_array_t *cache_array = malloc(sizeof(cache_array_t));
    cache->array[i] = cache_array;
    cache->array[i]->index = 0;
    cache->array[i]->LRU = 0;
    cache->array[i]->valid = 0;
    cache->array[i]->tag = 0;
    cache->array[i]->dirtybit = 0;
  }
  return cache;
}

//Destroys a given cache
void cache_destroy(cache_t *cache)
{
  int i;
  for(i = 0; i<cache->sets * cache->associativity; i++){
    free(cache->array[i]);
  }
  free(cache->array);
  free(cache);
}

//Set up index values and LRU-values for all element in the array
void set_index_lru(cache_t *cache)
{ 
  int i = 0;
  int k = 0;

  //Set a LRU-value for every array position for each index
  while(i < (cache->sets * cache->associativity)){
    for(int j = 0; j< cache->associativity; j++){
      cache->array[i]->LRU = j;
      cache->array[i]->index = k;
      i++;
    }
    k++;
  }
}

//Function for checking if a given adress exists in the given cache
int cache_contains(cache_t *cache, unsigned int adress) 
{
  //DIVIDING ADRESS INTO TAG, INDEX and OFFSET
  int tagshift = 32 - cache->tag_bitsize;               //Calculating how many steps the tag needs to be shifted
  int indexshift = 32 - cache->index_bitsize;           //Calculating how many steps the index needs to be shifted
  int offsetshift = 32 - cache->offset_bitsize;         //Calculating how many steps the offset need to be shifted

  int calculatetag = 4294967295 << cache->tag_bitsize;                                        //Making a number for & operation with adress, to get the tag bits
  int calculateindex = (4294967295 >> (32-cache->index_bitsize)) << cache->offset_bitsize;    //Making a number for & operation with adress, to get the index bits
  int calculateoffset = 4294967295 >> (32-cache->offset_bitsize);                             //Making a number for & operation woth adress, to get the offset bits

  int adresstag = adress&calculatetag;          //Assigning the tagnumber
  int adressindex = adress&calculateindex;      //Assigning the indexnumber
  int adressoffset = adress & calculateoffset;  //Assigning the offsetnumber

  int i = 0;

  while(i<(cache->sets * cache->associativity)){          //Going through all the sets and blocks in the cache
    if(cache->array[i]->index == adressindex){            //Check if the index in the cache matches the index in the given adress
      if(cache->array[i]->valid == 1){                    //Check if the valid bit in the cache is set to 1 
        if(cache->array[i]->tag == adresstag){            //Check if the tag in the cache matches the tag in the given adress
        //DO ALOT OF STUFF AND RETURN 1
          int highest_lru = cache->array[i]->LRU;         //Keeping the LRU-value at the current array position
          int lru_pos = i;                                //Keeping the position value
          int start_index = i - (i%cache->associativity); //Keeping the current position in index

          for(i=0; i<cache->associativity; i++){          //Update all the lru-values for the given index
            if(cache->array[start_index]->LRU < highest_lru){
              cache->array[start_index]->LRU++;
            }
            else if(cache->array[start_index]->LRU > highest_lru){
              cache->array[lru_pos]->LRU++;
              lru_pos = start_index;
              highest_lru = cache->array[start_index]->LRU;
            }
            start_index++;
          }
          cache->array[lru_pos]->LRU = 0;  //Set the LRU-value for the accesed element to 0
          return 1;
      }
    }
  }
    i++;
  }
  return 0;
}

//Adds a given adress to the cache when a miss has occured
void cache_add(cache_t *cache, unsigned int adress) 
{
  //DIVIDING ADRESS INTO TAG, INDEX and OFFSET
  int highest_lru;
  int lru_pos;
  int start_index;

  int tagshift = 32 - cache->tag_bitsize;               //Calculating how many steps the tag needs to be shifted
  int indexshift = 32 - cache->index_bitsize;           //Calculating how many steps the index needs to be shifted
  int offsetshift = 32 - cache->offset_bitsize;         //Calculating how many steps the offset need to be shifted

  int calculatetag = 4294967295 << cache->tag_bitsize;                                        //Making a number for & operation with adress, to get the tag bits
  int calculateindex = (4294967295 >> (32-cache->index_bitsize)) << cache->offset_bitsize;    //Making a number for & operation with adress, to get the index bits
  int calculateoffset = 4294967295 >> (32-cache->offset_bitsize);                             //Making a number for & operation woth adress, to get the offset bits

  int adresstag = adress&calculatetag;          //Assigning the tagnumber
  int adressindex = adress&calculateindex;      //Assigning the indexnumber
  int adressoffset = adress & calculateoffset;  //Assigning the offsetnumber
  
  int i = 0;

  while(cache->array[i]->index != adressindex){  //Find the first valid index in the array and set "i" to be this value
    i++;
  }

  //Store the array position of the valid index and its LRU-value
  if(i < cache->sets * cache->associativity){
    start_index = i;
    highest_lru = cache->array[i]->LRU;           //Store the array position where the valid index is
    lru_pos = start_index;

    //Go through all element in the array with the valid index, and find the array position that has the higest LRU-value
    while(cache->array[i]->index == adressindex){   //While the array index matches the given adresses index
      if(cache->array[i]->LRU > highest_lru){       //check if the LRU-value in the array is bigger than the current lru-value
        highest_lru = cache->array[i]->LRU;         //Set the higest lru value to the lru-value in the array
        lru_pos = i;                                //Updating the lru position
      }
      i++;
      if(i < cache->sets * cache->associativity){   //If "i" is less than all sets and blocks in the cache
        break;                                      //quit the loop
      }
    }
    //Read data from lower memory into the cache block and set valid bit to 1
    //IF the block is not changed (if the dirtybit is 0)
    if(cache->array[lru_pos]->dirtybit == 0){ //If the memory is not changed
      cache->array[lru_pos]->valid = 1;       //array valid bit is set to 1, meaning the block now has an element
      cache->array[lru_pos]->tag = adresstag; //Setting the tag in the cache to match the tag in the adress
    }
    else if(cache->array[lru_pos]->dirtybit == 1){  //If the memory is changed
      if(cache->next != NULL){                      //If the next cache level is not empty
        unsigned int new_tag;                       //For keeping the new tag
        unsigned int new_offset;                    //For keeping the new offset
        unsigned int new_index;                     //For keeping the new index 
        unsigned int new_adress;                    //For keeping the new adress
        new_tag = cache->array[lru_pos]->tag << (cache->index_bitsize + cache->offset_bitsize);
        new_index = cache->array[lru_pos]->index << cache->offset_bitsize;
        new_adress = new_tag + new_index;
        cache_add(cache->next, new_adress);
      }
    }
    //Increase all lru values on the given index, but keep the values lower than the number of way-associativities
    for(i = 0; i < cache->associativity; i++){
      cache->array[start_index]->LRU = (cache->array[start_index]->LRU +1) % cache->associativity;
      start_index++;
    }
  }
}

//Sets dirtybit to "1" if the blockdata in the cache is modified, "0" if not modified
void set_dirtybit(cache_t *cache, unsigned int adress, int dirtybit)
{
  //DIVIDING ADRESS INTO TAG, INDEX and OFFSET
  int tagshift = 32 - cache->tag_bitsize;               //Calculating how many steps the tag needs to be shifted
  int indexshift = 32 - cache->index_bitsize;           //Calculating how many steps the index needs to be shifted
  int offsetshift = 32 - cache->offset_bitsize;         //Calculating how many steps the offset need to be shifted

  int calculatetag = 4294967295 << cache->tag_bitsize;                                        //Making a number for & operation with adress, to get the tag bits
  int calculateindex = (4294967295 >> (32-cache->index_bitsize)) << cache->offset_bitsize;    //Making a number for & operation with adress, to get the index bits
  int calculateoffset = 4294967295 >> (32-cache->offset_bitsize);                             //Making a number for & operation woth adress, to get the offset bits

  int adresstag = adress&calculatetag;          //Assigning the tagnumber
  int adressindex = adress&calculateindex;      //Assigning the indexnumber
  int adressoffset = adress & calculateoffset;  //Assigning the offsetnumber
  
  int i = 0;

  //Check if adress exists in the cache by checking if the valid tag, index and tag is valid
  //Returns 1 if adress exists in the cache
  while(i < (cache->sets * cache->associativity)){
    if(cache->array[i]->index == adressindex){
      if(cache->array[i]->valid == 1){
        if(cache->array[i]->tag == adresstag){
          while(cache->array[i]->LRU != 0){
            i++;
          }
          cache->array[i]->dirtybit = dirtybit;
          break;
        }
      }
    }
    i++;
  }
}

//Initializes memory subsystem
void memory_init(void) 
{  
  L1instructionCache = cache_create(L1instr_CacheSize, L1instr_Associativity, L1instr_BlockSize);
  set_index_lru(L1instructionCache);
  L1instructionCache->next = L2unifiedCache;
  L1dataCache = cache_create(L1data_CacheSize, L1data_Associativity, L1data_BlockSize);
  set_index_lru(L1dataCache);
  L1dataCache->next = L2unifiedCache;
  L2unifiedCache = cache_create(L2_CacheSize, L2_Associativity, L2_BlockSize);
  set_index_lru(L2unifiedCache);
  L2unifiedCache->next = NULL;
    
  instr_count = 0;
}

//Checks if a given adress is in the L1 instruction cache and the L2 cache, and increments the corresponding caches "cachehit" or "cachemiss"
void memory_fetch(unsigned int address, data_t *data) 
{
  printf("memory: fetch 0x%08x\n", address);
  if(cache_contains(L1instructionCache, address) == 1){       //If the adress exists in the cache
    L1instructionCache->cachehit++;                           //Increment its hit count
  }
  else if(cache_contains(L1instructionCache, address) == 0){  //If the adress is not in the cache
    L1instructionCache->cachemiss++;                          //Increment its miss count
    cache_add(L1instructionCache, address);                   //Add the adress to the cache
    if(cache_contains(L2unifiedCache, address) == 1){         //Check if the adress is in the next level cache
      L2unifiedCache->cachehit++;                             //If so increment its hit count
    }
    else if(cache_contains(L2unifiedCache, address) == 0){    //If the adress is not in the next level cache
      cache_add(L2unifiedCache, address);                     //Add the adress
    }
  }

  instr_count++;
}

//Checks if a given adress is in the L1 data cache and the L2 cache, and increments the corresponding caches "cachehit" or "cachemiss"
void memory_read(unsigned int address, data_t *data)
{
  printf("memory: read 0x%08x\n", address);

  if(cache_contains(L1dataCache, address) == 1){            //If L1data cache contains the adress
    L1dataCache->cachehit++;                                //Increment its hit count
  }
  else if(cache_contains(L1dataCache, address) == 0){       //If adress is not in the L1data cache
    L1dataCache->cachemiss++;                               //Increment its miss count
    cache_add(L1dataCache, address);                        //Add the adress to the cache
    set_dirtybit(L1dataCache, address, 0);                  //Set dirty bit to "0" indicating that the memory is NOT modified
    if(cache_contains(L2unifiedCache, address) == 1){       //If adress is in the L2 cache
      L2unifiedCache->cachehit++;                           //Increment lv2 caches hit count
    }
    else if(cache_contains(L2unifiedCache, address) == 0){  //If adress is not in the L2 cache
      L2unifiedCache->cachemiss++;                          //Increment its miss count
      cache_add(L2unifiedCache, address);                   //Add the adress to the cache
      set_dirtybit(L2unifiedCache, address, 0);             //Set dirtybit to "0" indicating that the data is not modified
    }
  }
  
  instr_count++;
}

//Write-back policy, only writing back to memory IF a block in the cache needs to be replaced
void memory_write(unsigned int address, data_t *data)
{
  printf("memory: write 0x%08x\n", address);

  if(cache_contains(L1dataCache, address) == 1){      //If adress we want to write is already inn the L1data cache
    set_dirtybit(L1dataCache, address, 1);            //Set the corresponding dirty bit to "1" indicating that it ahs been modified
    L1dataCache->cachehit++;                          //Increment the caches hit count
  }
  else if(cache_contains(L1dataCache, address) == 0){ //If adress we want to write is not in the cache
    cache_add(L1dataCache, address);                  //add this write/adress to the cache
    set_dirtybit(L1dataCache, address, 1);            //set dirtybit to "1" indicating that the adress has been modified
  }
  
  instr_count++;
}

//Deinitializes memory subsystem
void memory_finish(void)  
{
  fprintf(stdout, "Executed %lu instructions.\n\n", instr_count);
  
  unsigned int hitrate_L1instructioncache;
  unsigned int hitrate_L1datacache;
  unsigned int hitrate_L2cache;
  double hit_L1instruction;
  double hit_L1data;
  double hit_L2unified;

  hitrate_L1instructioncache = L1instructionCache->cachehit;
  hitrate_L1datacache = L1dataCache->cachehit;
  hitrate_L2cache = L2unifiedCache->cachehit;

  hit_L1instruction = ((double)hitrate_L1instructioncache / (double)instr_count)*100;
  hit_L1data = ((double)hitrate_L1datacache / (double)instr_count)*100;
  hit_L2unified = ((double)hitrate_L2cache / (double)instr_count)*100;

  fprintf(stdout, "Hitrate level one instruction cache: %u, %f%c \n", hitrate_L1instructioncache, hit_L1instruction, '%');
  fprintf(stdout, "Hitrate level one data cache: %u, %f%c \n", hitrate_L1datacache, hit_L1data, '%');
  fprintf(stdout, "Hitrate level two cache: %u, %f%c \n", hitrate_L2cache, hit_L2unified, '%');

  cache_destroy(L1instructionCache);
  cache_destroy(L1dataCache);
  cache_destroy(L2unifiedCache);
}
