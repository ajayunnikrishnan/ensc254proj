#include "dogfault.h"
#include "cache.h"
#include <assert.h>
#include <ctype.h>
#include <getopt.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include "config.h"

// HELPER FUNCTIONS USEFUL FOR IMPLEMENTING THE CACHE

unsigned long long address_to_block(const unsigned long long address, const Cache *cache) {
    /*YOUR CODE HERE*/
}

unsigned long long cache_tag(const unsigned long long address, const Cache *cache) {
    /*YOUR CODE HERE*/
}

unsigned long long cache_set(const unsigned long long address, const Cache *cache) {
    /*YOUR CODE HERE*/
}

bool probe_cache(const unsigned long long address, const Cache *cache) {
    /*YOUR CODE HERE*/
}

void hit_cacheline(const unsigned long long address, Cache *cache) {
    /*YOUR CODE HERE*/

    // Did not find line, not supposed to be here
    assert(0);
}

bool insert_cacheline(const unsigned long long address, Cache *cache) {
    /*YOUR CODE HERE*/

    
}

unsigned long long victim_cacheline(const unsigned long long address, const Cache *cache) {
    /*YOUR CODE HERE*/
}

void replace_cacheline(const unsigned long long victim_block_addr, const unsigned long long insert_addr, Cache *cache) {
    /*YOUR CODE HERE*/
    // Not supposed to be here
    assert(0);
}

void cacheSetUp(Cache *cache, char *name) {
    cache->hit_count = 0;
    /*YOUR CODE HERE*/
}

void deallocate(Cache *cache) {
    /*YOUR CODE HERE*/
}

result operateCache(const unsigned long long address, Cache *cache) {
    result r;
    /*YOUR CODE HERE*/
    #ifdef PRINT_CACHE_TRACES 
    printf(CACHE_HIT_FORMAT, address); 
    #endif 
    return r;
    /*YOUR CODE HERE*/
}

int processCacheOperation(unsigned long address, Cache *cache) {
    result r;
    /*YOUR CODE HERE*/
    if (r.status == CACHE_HIT) {
        return CACHE_HIT_LATENCY;
    } else if (r.status == CACHE_MISS)
    {
        return CACHE_MISS_LATENCY;
    }
    else
    {
        return CACHE_OTHER_LATENCY;
    }
}
