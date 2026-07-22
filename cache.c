#include "cache.h"
#include "dogfault.h"
#include <assert.h>
#include <ctype.h>
#include <getopt.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// DO NOT MODIFY THIS FILE. INVOKE AFTER EACH ACCESS FROM runTrace
void print_result(result r) {
  if (r.status == CACHE_EVICT)
    printf(" [status: miss eviction, victim_block: 0x%llx, insert_block: 0x%llx]",
           r.victim_block_addr, r.insert_block_addr);
  if (r.status == CACHE_HIT)
    printf(" [status: hit]");
  if (r.status == CACHE_MISS)
    printf(" [status: miss, insert_block: 0x%llx]", r.insert_block_addr);
}

/* This is the entry point to operate the cache for a given address in the trace file.
 * First, is increments the global lru_clock in the corresponding cache set for the address.
 * Second, it checks if the address is already in the cache using the "probe_cache" function.
 * If yes, it is a cache hit:
 *     1) call the "hit_cacheline" function to update the counters inside the hit cache 
 *        line, including its lru_clock and access_counter.
 *     2) record a hit status in the return "result" struct and update hit_count 
 * Otherwise, it is a cache miss:
 *     1) call the "insert_cacheline" function, trying to find an empty cache line in the
 *        cache set and insert the address into the empty line. 
 *     2) if the "insert_cacheline" function returns true, record a miss status and the
          inserted block address in the return "result" struct and update miss_count
 *     3) otherwise, if the "insert_cacheline" function returns false:
 *          a) call the "victim_cacheline" function to figure which victim cache line to 
 *             replace based on the cache replacement policy (LRU and LFU).
 *          b) call the "replace_cacheline" function to replace the victim cache line with
 *             the new cache line to insert.
 *          c) record an eviction status, the victim block address, and the inserted block
 *             address in the return "result" struct. Update miss_count and eviction_count.
 */
result operateCache(const unsigned long long address, Cache *cache) {
  /* YOUR CODE HERE */
  // 1.) this access touches the set, so bump the set's global LRU clock first
  unsigned long long set_idx = cache_set(address, cache);
  cache->sets[set_idx].lru_clock++;

  result r;

  // 2.) we check if the address is already cached
  
  if(probe_cache(address, cache)) {
    // if HIT
    hit_cacheline(address, cache);
    r.status = CACHE_HIT;
    r.victim_block_addr = 0; //unused for HIT
    r.insert_block_addr = 0; //unused for HIT
    cache->hit_count++;
    
    return r;
  }

  // if MISS: place into an empty line first
  if (insert_cacheline(address, cache)) {
    r.status = CACHE_MISS;
    r.victim_block_addr = 0;   // no victim, nothing was evicted
    r.insert_block_addr = address_to_block(address, cache);
    cache->miss_count++;
    return r;
  }

  //MISS but with eviction (if the set is full we must replace the victim)
  unsigned long long victim_block_addr = victim_cacheline(address, cache);
  replace_cacheline(victim_block_addr, address, cache);
 
  r.status = CACHE_EVICT;
  r.victim_block_addr = victim_block_addr;
  r.insert_block_addr = address_to_block(address, cache);
  cache->miss_count++;
  cache->eviction_count++;
  return r;
}

// HELPER FUNCTIONS USEFUL FOR IMPLEMENTING THE CACHE
// Given an address, return the block (aligned) address,
// i.e., byte offset bits are cleared to 0
unsigned long long address_to_block(const unsigned long long address,
                                const Cache *cache) {
  /* YOUR CODE HERE */

  //clear the low blockBits bits who's the byte offset in the block
  return address & ~((1ULL << cache->blockBits) - 1ULL);
}

// Return the cache tag of an address
unsigned long long cache_tag(const unsigned long long address,
                             const Cache *cache) {
  /* YOUR CODE HERE */

  //Tag is everything above the set-index bits and block-offset bits
  return address >> (cache->setBits + cache->blockBits);
}

// Return the cache set index of the address
unsigned long long cache_set(const unsigned long long address,
                             const Cache *cache) {
  /* YOUR CODE HERE */
  // Shift out the block offset bits, then keep only the low `setBits` bits.
  // Note: when setBits == 0 (fully associative), (1ULL << 0) - 1 == 0,
  // so the mask correctly forces set index 0 for every address
  unsigned long long set_mask = (1ULL << cache->setBits) - 1ULL;
  return (address >> cache->blockBits) & set_mask;

}

// Check if the address is found in the cache. If so, return true. else return false.
bool probe_cache(const unsigned long long address, const Cache *cache) {
  /* YOUR CODE HERE */
  unsigned long long set_idx = cache_set(address, cache);
  unsigned long long tag = cache_tag(address, cache);
  Set *set = &cache->sets[set_idx];
 
  for (int i = 0; i < cache->linesPerSet; i++) {
    if (set->lines[i].valid && set->lines[i].tag == tag) {
      return true;
    }
  }
  return false;
}

// Access address in cache. Called only if probe is successful.
// Update the LRU (least recently used) or LFU (least frequently used) counters.
void hit_cacheline(const unsigned long long address, Cache *cache){
  /* YOUR CODE HERE */
  unsigned long long set_idx = cache_set(address, cache);
  unsigned long long tag = cache_tag(address, cache);
  Set *set = &cache->sets[set_idx];
 
  for (int i = 0; i < cache->linesPerSet; i++) {
    if (set->lines[i].valid && set->lines[i].tag == tag) {
      // Mark this line as the most recently used in its set...
      set->lines[i].lru_clock = set->lru_clock;
      //and now record that it has been accessed again (for LFU configuration).
      set->lines[i].access_counter++;
      return;
    }
  }
 }

/* This function is only called if probe_cache returns false, i.e., the address is
 * not in the cache. In this function, it will try to find an empty (i.e., invalid)
 * cache line for the address to insert. 
 * If it found an empty one:
 *     1) it inserts the address into that cache line (marking it valid).
 *     2) it updates the cache line's lru_clock based on the global lru_clock 
 *        in the cache set and initiates the cache line's access_counter.
 *     3) it returns true.
 * Otherwise, it returns false.  
 */ 
bool insert_cacheline(const unsigned long long address, Cache *cache) {
  /* YOUR CODE HERE */
   unsigned long long set_idx = cache_set(address, cache);
  Set *set = &cache->sets[set_idx];
 
  for (int i = 0; i < cache->linesPerSet; i++) {
    if (!set->lines[i].valid) {
      set->lines[i].valid = true;
      set->lines[i].tag = cache_tag(address, cache);
      set->lines[i].block_addr = address_to_block(address, cache);
      set->lines[i].lru_clock = set->lru_clock;
      set->lines[i].access_counter = 1;  // this counts as the first access
      return true;
    }
  }
   return false;
}

// If there is no empty cacheline, this method figures out which cacheline to replace
// depending on the cache replacement policy (LRU and LFU). It returns the block address
// of the victim cacheline; note we no longer have access to the full address of the victim
unsigned long long victim_cacheline(const unsigned long long address,
                                const Cache *cache) {
  /* YOUR CODE HERE */
  
  unsigned long long set_idx = cache_set(address, cache);
  Set *set = &cache->sets[set_idx];
 
  int victim = 0;
  for (int i = 1; i < cache->linesPerSet; i++) {
    if (cache->lfu) {
      // LFU: prefer the smallest access_counter; break ties with smallest lru_clock.
      if (set->lines[i].access_counter < set->lines[victim].access_counter ||
          (set->lines[i].access_counter == set->lines[victim].access_counter &&
           set->lines[i].lru_clock < set->lines[victim].lru_clock)) {
        victim = i;
      }
    } else {
      // LRU: prefer the smallest lru_clock (least recently used).
      if (set->lines[i].lru_clock < set->lines[victim].lru_clock) {
        victim = i;
      }
    }
  }
  return set->lines[victim].block_addr;
}

/* Replace the victim cacheline with the new address to insert. Note for the victim cachline,
 * we only have its block address. For the new address to be inserted, we have its full address.
 * Remember to update the new cache line's lru_clock based on the global lru_clock in the cache
 * set and initiate the cache line's access_counter.
 */
void replace_cacheline(const unsigned long long victim_block_addr,
		       const unsigned long long insert_addr, Cache *cache) {
  /* YOUR CODE HERE */
   unsigned long long set_idx = cache_set(insert_addr, cache);
  Set *set = &cache->sets[set_idx];
 
  for (int i = 0; i < cache->linesPerSet; i++) {
    if (set->lines[i].valid && set->lines[i].block_addr == victim_block_addr) {
      set->lines[i].tag = cache_tag(insert_addr, cache);
      set->lines[i].block_addr = address_to_block(insert_addr, cache);
      set->lines[i].lru_clock = set->lru_clock;
      set->lines[i].access_counter = 1;  // fresh line, first access
      return;
    }
  }
}

// allocate the memory space for the cache with the given cache parameters
// and initialize the cache sets and lines.
// Initialize the cache name to the given name 
void cacheSetUp(Cache *cache, char *name) {
  /* YOUR CODE HERE */
    unsigned long long num_sets = 1ULL << cache->setBits;
 
  cache->sets = (Set *)malloc(num_sets * sizeof(Set));
  assert(cache->sets != NULL);
 
  for (unsigned long long i = 0; i < num_sets; i++) {
    cache->sets[i].lru_clock = 0;
    cache->sets[i].lines = (Line *)malloc(cache->linesPerSet * sizeof(Line));
    assert(cache->sets[i].lines != NULL);
 
    for (int j = 0; j < cache->linesPerSet; j++) {
      cache->sets[i].lines[j].valid = false;
      cache->sets[i].lines[j].tag = 0;
      cache->sets[i].lines[j].block_addr = 0;
      cache->sets[i].lines[j].lru_clock = 0;
      cache->sets[i].lines[j].access_counter = 0;
    }
  }
 
  cache->name = name;
  
}

// deallocate the memory space for the cache
void deallocate(Cache *cache) {
  /* YOUR CODE HERE */
  unsigned long long num_sets = 1ULL << cache->setBits;
  for (unsigned long long i = 0; i < num_sets; i++) {
    free(cache->sets[i].lines);
  }
  free(cache->sets);
  cache->sets = NULL;
}

// print out summary stats for the cache
void printSummary(const Cache *cache) {
  printf("%s hits: %d, misses: %d, evictions: %d\n", cache->name, cache->hit_count, cache->miss_count, cache->eviction_count);
}
  
