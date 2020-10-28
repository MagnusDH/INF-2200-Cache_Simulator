
#include "memory.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

// Cache parameters
#define l1_instr_size 32
#define l1_instr_blocksize 64
#define l1_instr_assosiativity 4
#define l1_instr_policy 1

#define l1_data_size 32
#define l1_data_blocksize 64
#define l1_data_assosiativity 8
#define l1_data_policy 1

#define l2_size 256
#define l2_blocksize 64
#define l2_assosiativity 8
#define l2_policy 1

// Instruction counter
static unsigned long instr_count;

// Typedef-ing structures
typedef struct cache cache_t;
typedef struct info info_t;

// Global variables representing each cache structure
static cache_t *cache_one_instr, *cache_one_data, *cache_two;

// Structure for each cache block
struct info {
  unsigned int index;
  unsigned int lru;
  unsigned int valid;
  unsigned int tag;
  unsigned int dirtybit;
};

// Structure for each cache
struct cache{
  info_t **array;
  unsigned int size;
  unsigned int blocksize;
  unsigned int index_sets;
  unsigned int tag_bitsize;
  unsigned int index_bitsize;
  unsigned int offset_bitsize;
  int associativity;
  unsigned int hit;
  unsigned int miss;
  int policy;
  cache_t *next;
};

/*
 * Create a cache with given cache-size(kb), block size (B), associativity(int), policy(int)
 */
static cache_t *cache_create(unsigned int size, unsigned int block_size, int associative, int policy)
{
  // Allocating memory for cache structure
  cache_t *cache = malloc(sizeof(cache_t));
  if(cache == NULL){
    return NULL;
  }
  unsigned int new_size, offset_bits, index_size, index_bits;
  // Finding the cache size in bytes
  new_size = size * 128;
  // Finding the amount of bits needed in cache structure
  offset_bits = log2(block_size * 4);
  index_size = new_size / (block_size * associative);
  index_bits = log2(index_size);

  cache->hit = 0;
  cache->miss = 0;
  cache->size = new_size;
  cache->blocksize = block_size;
  cache->associativity = associative;
  cache->offset_bitsize = offset_bits;
  cache->index_sets = index_size;
  cache->index_bitsize = index_bits;
  cache->tag_bitsize = 32 - index_bits - offset_bits;
  cache->policy = policy;

  // Allocate memory for an empty array
  cache->array = calloc(new_size, sizeof(info_t));
  if(cache->array == NULL){
    if(cache != NULL){
      free(cache);
    }
    return NULL;
  }

  // Allocate memory for the structure in each array position in the cache
  for(int i = 0; i < index_size * associative; i++){
    info_t *info = malloc(sizeof(info_t));
    cache->array[i] = info;
    cache->array[i]->index = 0;
    cache->array[i]->lru = 0;
    cache->array[i]->valid = 0;
    cache->array[i]->tag = 0;
    cache->array[i]->dirtybit = 0;
  }
  return cache;
}

// Set up index values and lru values
// for all elements in the array
static void set_index_lru(cache_t *cache)
{
  int i = 0;
  int k = 0;
  // Set an unique lru value for evey array-position for each index
  while(i < (cache->index_sets * cache->associativity) ){
    for(int j = 0; j < cache->associativity; j++){
      cache->array[i]->lru = j;
      cache->array[i]->index = k;
      i++;
    }
    k++;
  }
}

/* Initializing memory subsystem*/
void memory_init(void)
{
  // Allocate memory for the three caches,
  // and set up the index values, and make each cache point
  // to lower memory
  cache_one_instr = cache_create(l1_instr_size, l1_instr_blocksize, l1_instr_assosiativity, l1_instr_policy);
  set_index_lru(cache_one_instr);
  cache_one_instr->next = cache_two;

  cache_one_data = cache_create(l1_data_size, l1_data_blocksize, l1_data_assosiativity, l1_data_policy);
  set_index_lru(cache_one_data);
  cache_one_data->next = cache_two;

  cache_two = cache_create(l2_size, l2_blocksize, l2_assosiativity, l2_policy);
  set_index_lru(cache_two);
  cache_two->next = NULL;

  // Set instruction_counter to 0
  instr_count = 0;
}

/*
 * Function checking if given address
 * exists in the cache.
 * Returns 1 if address is in cache and 0 if not.
 * If address exists we update the lru values
 */
static int cache_contains(cache_t *cache, unsigned int address)
{
  int i = 0;
  unsigned int addr_index, addr_tag;
  // Define the starting bit of the index
  int start = cache->offset_bitsize;
  // Define the start of the tag (end of the index)
  int end = 32 - cache->tag_bitsize;
  // Set all bits below the end to 1's
  int bitmask = (1 << (end - start)) - 1;

  // Slice out the index and tag from the address
  addr_index = (address >> start) & bitmask;
  addr_tag = address >> (cache->offset_bitsize + cache->index_bitsize);

  // Iterate over the cache array
  while(i < (cache->index_sets * cache->associativity) ){
    // Check if the given index matches
    // the current array index
    if(cache->array[i]->index == addr_index){
      // Check if the tag matches
      if(cache->array[i]->tag == addr_tag){
        // Reset counter to the beginning of the current index
        i = i - (i % cache->associativity);
        int start_index = i;
        int counter;

        // Find the highest lru value in the current index,
        // and check the valid bit of the block
        for(counter = 0; counter < cache->associativity; counter++, i++){
          if(cache->array[i]->lru == cache->associativity - 1){
            if(cache->array[i]->valid == 1){
              break;
            }
          }
        }
        // Define the start of the current index
        int index_lenght = start_index + cache->associativity;
        // Update all the lru values in the current index
        for(start_index; start_index < index_lenght; start_index++){
          cache->array[start_index]->lru = (cache->array[start_index]->lru + 1) % cache->associativity;
        }
        return 1;
      }
    }
    i++;
  }
  return 0;
}

/*
 * Function for read-operation for write-through policy
 */
static void cache_wt_read(cache_t *cache, unsigned int address)
{
  int i = 0;
  int start_index;
  unsigned int addr_index, addr_tag;
  // Define the starting bit of the index
  int start = cache->offset_bitsize;
  // Define the start of the tag (end of the index)
  int end = 32 - cache->tag_bitsize;
  // Set all bits below the end to 1's
  int bitmask = (1 << (end - start)) - 1;

  // Slice out the index and tag from the address
  addr_index = (address >> start) & bitmask;
  addr_tag = address >> (cache->offset_bitsize + cache->index_bitsize);

  // Iterate through the array of the cache
  while(i < cache->associativity * cache->index_sets){
    // Check if the current index matches the
    // wanted index of the address
    if(cache->array[i]->index == addr_index){
      // Store the start of the current index in the array
      start_index = i - (i % cache->associativity);
      int j;

      // Find the highest lru value
      for(j = 0; j < cache->associativity; j++){
        if(cache->array[i]->lru == cache->associativity - 1){
          break;
        }
      }
      // Give values for valid bit, dirtybit, and tag
      cache->array[i]->valid = 1;
      cache->array[i]->dirtybit = 0;
      cache->array[i]->tag = addr_tag;

      // Update all the lru-values on the current index
      for(start_index; start_index < cache->associativity; start_index++){
        cache->array[start_index]->lru = (cache->array[start_index]->lru + 1) % cache->associativity;
      }
    }
    i++;
  }
}

/*
 * Function for write-operation for write through
 */
static void cache_wt_write(cache_t *cache, unsigned int address)
{
  int i = 0;
  int start_index;
  unsigned int addr_index, addr_tag;
  // Define the starting bit of the index
  int start = cache->offset_bitsize;
  // Define the start of the tag (end of the index)
  int end = 32 - cache->tag_bitsize;
  // Set all bits below the end to 1's
  int bitmask = (1 << (end - start)) - 1;

  // Slice out the index and tag from the address
  addr_index = (address >> start) & bitmask;
  addr_tag = address >> (cache->offset_bitsize + cache->index_bitsize);

  // Iterate through the array of the given cache
  while(i < cache->associativity * cache->index_sets){
    // Check if the current index matches the
    // wanted index from the address
    if(cache->array[i]->index == addr_index){
      start_index = i - (i % cache->associativity);
      int j;
      // Find the position of the highest lru-value
      for(j = 0; j < cache->associativity; j++){
        if(cache->array[i]->lru == cache->associativity - 1){
          break;
        }
      }
      // Set new values for the tag, validbit and dirtybit,
      // on the least recently used block of the index
      cache->array[i]->tag = addr_tag;
      cache->array[i]->valid = 1;
      cache->array[i]->dirtybit = 1;

      // Write the data to the next cache if there is one
      if(cache->next != NULL){
        unsigned int new_tag, new_offset, new_index, new_address;
        // Find new values for tag, index and address, and write to lower memory
        new_tag = cache->array[i]->tag << (cache->index_bitsize + cache->offset_bitsize);
        new_index = cache->array[i]->index << cache->offset_bitsize;
        new_address = new_tag + new_index;
        cache_wt_write(cache->next, new_address);
      }

      // Update the lru-values on the current index
      for(start_index; start_index < cache->associativity; start_index++){
        cache->array[start_index]->lru = (cache->array[start_index]->lru + 1) % cache->associativity;
      }
    }
  i++;
  }
}

/*
 * Function for adding new address into cache when
 * a miss has occured, using write-back
 */
static void cache_add(cache_t *cache, unsigned int address)
{
  int i = 0;
  int start_index;
  unsigned int addr_index, addr_tag;
  // Define the starting bit of the index
  int start = cache->offset_bitsize;
  // Define the start of the tag (end of the index)
  int end = 32 - cache->tag_bitsize;
  // Set all bits below the end to 1's
  int bitmask = (1 << (end - start)) - 1;

  // Slice out the index and tag from the address
  addr_index = (address >> start) & bitmask;
  addr_tag = address >> (cache->offset_bitsize + cache->index_bitsize);

  // Iterate through the array of the given cache
  while(i < cache->associativity * cache->index_sets){
    // Check if the current array-index matches
    // the wanted index from the address
    if(cache->array[i]->index == addr_index){
      int start_index = i - (i % cache->associativity);
      int j;

      // Find the least recently used block and store
      // its array-position
      for(j = 0; j < cache->associativity; j++){
        if(cache->array[i]->lru == cache->associativity - 1){
          break;
        }
      }
      // If the block is nt dirty we write values
      // straight into the block and mark it as
      // dirty.
      if(cache->array[i]->dirtybit == 0){
        cache->array[i]->valid = 1;
        cache->array[i]->dirtybit = 1;
        cache->array[i]->tag = addr_tag;
      }
      // If the block is dirty we check for lower memory cache
      else if(cache->array[i]->dirtybit == 1){
        if(cache->next != NULL){
          unsigned int new_tag, new_offset, new_index, new_address;
          // Calculate the address based on the tag and index for the dirty block,
          // and write it to the lower memory cache
          new_tag = cache->array[i]->tag << (cache->index_bitsize + cache->offset_bitsize);
          new_index = cache->array[i]->index << cache->offset_bitsize;
          new_address = new_tag + new_index;
          cache_add(cache->next, new_address);
          cache->array[i]->valid = 1;
          cache->array[i]->dirtybit = 1;
          cache->array[i]->tag = addr_tag;
        }
      }
      // Update the lru-values for the given index
      for(start_index; start_index < cache->associativity; start_index++){
        cache->array[start_index]->lru = (cache->array[start_index]->lru + 1) % cache->associativity;
      }
    }
    i++;
  }
}


/*
 * Function for setting a dirtybit value to a block
 */
static void set_dirtybit(cache_t *cache, unsigned int address, int dirtybit)
{
  int i = 0;
  unsigned int addr_index, addr_tag;
  // Define the starting bit of the index
  int start = cache->offset_bitsize;
  // Define the start of the tag (end of the index)
  int end = 32 - cache->tag_bitsize;
  // Set all bits below the end to 1's
  int bitmask = (1 << (end - start)) - 1;

  // Slice out the index and tag from the address
  addr_index = (address >> start) & bitmask;
  addr_tag = address >> (cache->offset_bitsize + cache->index_bitsize);


  // Iterate through the array for the given cache
  while(i < cache->index_sets * cache->associativity){
    // Check if the current index matches the
    // index from the address
    if(cache->array[i]->index == addr_index){
      // Find the most recently used block in the set
      for(int j = 0; j < cache->associativity; j++){
        if(cache->array[i + j]->lru == 0){
          // Give the dirtybit the new value
          cache->array[i + j]->dirtybit = dirtybit;
          return;
        }
      }
    }
    i++;
  }
}


/* Fetch addresses from trace file */
void memory_fetch(unsigned int address, data_t *data)
{
  printf("memory: fetch 0x%08x\n", address);

  // Check if address already is in the cache
  if(cache_contains(cache_one_instr, address) == 1){
    cache_one_instr->hit++;
  }
  // Check if address doesn't exist in the cache
  else if(cache_contains(cache_one_instr, address) == 0){
    cache_one_instr->miss++;
    // Check if write policy for cache is write-back
    if(cache_one_instr->policy == 1){
      // Read address into cache, and set dirtybit to 0
      cache_add(cache_one_instr, address);
      set_dirtybit(cache_one_instr, address, 0);
    }
    // Check if write policy is write-through
    else if(cache_one_instr->policy == 0){
      // Read address into cache
      cache_wt_read(cache_one_instr, address);
    }

    // Check if address already is in the cache
    if(cache_contains(cache_two, address) == 1){
      cache_two->hit++;
    }
    // Check if the address is not in the cache
    else if(cache_contains(cache_two, address) == 0){
      cache_two->miss++;
      // Check if write policy is write-back
      if(cache_two->policy == 1){
        // Read address into cache, and set
        // dirtybit to 0
        cache_add(cache_two, address);
        set_dirtybit(cache_one_instr, address, 0);
      }
      // Check if write policy is write-through
      else if(cache_two->policy == 0){
        cache_wt_read(cache_two, address);
      }
    }
  }
  instr_count++;
}



/* Read addresses from trace file */
void memory_read(unsigned int address, data_t *data)
{
  printf("memory: read 0x%08x\n", address);

  // Check if address already is in th cache
  if(cache_contains(cache_one_data, address) == 1){
    cache_one_data->hit++;
  }
  // Check if the address is not in the cache
  else if(cache_contains(cache_one_data, address) == 0){
    cache_one_data->miss++;
    // Check if the write policy is write-back
    if(cache_one_data->policy == 1){
      // Read data into cache, and set dirtybit to 0
      cache_add(cache_one_data, address);
      set_dirtybit(cache_one_data, address, 0);
    }
    // Check if write policy is write-through
    else if(cache_one_data->policy == 0){
      cache_wt_read(cache_one_data, address);
    }
    // Check if address already is in cache
    if(cache_contains(cache_two, address) == 1){
      cache_two->hit++;
    }
    // Check if address is not in the cache
    else if(cache_contains(cache_two, address) == 0){
      cache_two->miss++;
      // Check if write policy is write-back
      if(cache_two->policy == 1){
        // Read data into cache, and set dirtybit to 0
        cache_add(cache_two, address);
        set_dirtybit(cache_two, address, 0);
      }
      // Check if write policy is write-through
      else if(cache_two->policy == 0){
        // Read data into cache
        cache_wt_read(cache_two, address);
      }
    }
  }
  instr_count++;
}


/* Write adress from trace file into cache */
void memory_write(unsigned int address, data_t *data)
{
  printf("memory: write 0x%08x\n", address);

  // Check if address already is in the cache
  if(cache_contains(cache_one_data, address) == 1){
    cache_one_data->hit++;
    // Check if write policy is write-back
    if(cache_one_data->policy == 1){
      // Write data into cache, and set dirtybit to 1
      cache_add(cache_one_data, address);
      set_dirtybit(cache_one_data, address, 1);
    }
  }
  // Check if address is not already in the cache
  else if(cache_contains(cache_one_data, address) == 0){
    cache_one_data->miss++;
    // Check if write policy is write-back
    if(cache_one_data->policy == 1){
      // Write data into cache, and set dirtybit to 1
      cache_add(cache_one_data, address);
      set_dirtybit(cache_one_data, address, 1);
    }
    // Check if write policy is write-through
    else if(cache_one_data->policy == 0){
      // Write data into cache
      cache_wt_write(cache_one_data, address);
    }
  }
  instr_count++;
}


// Deallocate memory for cache
static void cache_destroy(cache_t *cache)
{
  // Free memory allocated for the structure in each array-element
  int i;
  for(i = 0; i < cache->index_sets * cache->associativity; i++){
    free(cache->array[i]);
  }
  // Free the memory allocated for the array,
  // and the memory allocated for the cache itself
  free(cache->array);
  free(cache);
}

/* Deinitialize memory subsystem */
void memory_finish(void)
{
  fprintf(stdout, "Executed %lu instructions.\n\n", instr_count);

  unsigned int hitrate_one_instr, hitrate_one_data, hitrate_two, instr_miss, data_miss, l2_miss;
  double hit_ins, hit_data, hit_two;

  // Store amount of hits for each cache
  hitrate_one_instr = cache_one_instr->hit;
  hitrate_one_data = cache_one_data->hit;
  hitrate_two = cache_two->hit;
  // Store total accesses for each cache
  instr_miss = (cache_one_instr->hit + cache_one_instr->miss);
  data_miss = (cache_one_data->hit + cache_one_data->miss);
  l2_miss = (cache_two->hit + cache_two->miss);
  // Find the hit percentage for each cache
  hit_ins = ((double)hitrate_one_instr / (double)instr_miss)*100;
  hit_data = ((double)hitrate_one_data / (double)data_miss)*100;
  hit_two = ((double)hitrate_two / (double)l2_miss)*100;

  // Output results
  fprintf(stdout, "Hitrate level one instruction cache: %u of %u instructions; %f%c \n", hitrate_one_instr, instr_miss, hit_ins, '%');
  fprintf(stdout, "Hitrate level one data cache: %u of %u instructions; %f%c \n", hitrate_one_data, data_miss, hit_data, '%');
  fprintf(stdout, "Hitrate level two cache: %u of %u instructions; %f%c \n", hitrate_two, l2_miss, hit_two, '%');

  // Deallocate memory that were allocated
  cache_destroy(cache_one_instr);
  cache_destroy(cache_one_data);
  cache_destroy(cache_two);
}
