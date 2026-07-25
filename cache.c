#include "dogfault.h"
#include "cache.h"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

extern uint64_t hit_count;
extern uint64_t miss_count;

unsigned long long address_to_block(const unsigned long long address, const Cache *cache) {
    return address & ~((1ULL << cache->blockBits) - 1ULL);
}

unsigned long long cache_tag(const unsigned long long address, const Cache *cache) {
    return address >> (cache->setBits + cache->blockBits);
}

unsigned long long cache_set(const unsigned long long address, const Cache *cache) {
    unsigned long long mask = (1ULL << cache->setBits) - 1ULL;
    return (address >> cache->blockBits) & mask;
}

bool probe_cache(const unsigned long long address, const Cache *cache) {
    unsigned long long set_idx = cache_set(address, cache);
    unsigned long long tag = cache_tag(address, cache);
    const Set *set = &cache->sets[set_idx];

    for (int i = 0; i < cache->linesPerSet; i++)
        if (set->lines[i].valid && set->lines[i].tag == tag)
            return true;

    return false;
}

void hit_cacheline(const unsigned long long address, Cache *cache) {
    unsigned long long set_idx = cache_set(address, cache);
    unsigned long long tag = cache_tag(address, cache);
    Set *set = &cache->sets[set_idx];

    for (int i = 0; i < cache->linesPerSet; i++) {
        if (set->lines[i].valid && set->lines[i].tag == tag) {
            set->lines[i].lru_clock = set->lru_clock;
            set->lines[i].access_counter++;
            return;
        }
    }

    assert(0);
}

bool insert_cacheline(const unsigned long long address, Cache *cache) {
    unsigned long long set_idx = cache_set(address, cache);
    Set *set = &cache->sets[set_idx];

    for (int i = 0; i < cache->linesPerSet; i++) {
        if (!set->lines[i].valid) {
            set->lines[i].valid = true;
            set->lines[i].tag = cache_tag(address, cache);
            set->lines[i].block_addr = address_to_block(address, cache);
            set->lines[i].lru_clock = set->lru_clock;
            set->lines[i].access_counter = 1;
            return true;
        }
    }

    return false;
}

unsigned long long victim_cacheline(const unsigned long long address, const Cache *cache) {
    const Set *set = &cache->sets[cache_set(address, cache)];
    int victim = 0;

    for (int i = 1; i < cache->linesPerSet; i++) {
        if (cache->lfu) {
            if (set->lines[i].access_counter <
                    set->lines[victim].access_counter ||
                (set->lines[i].access_counter ==
                     set->lines[victim].access_counter &&
                 set->lines[i].lru_clock <
                     set->lines[victim].lru_clock)) {
                victim = i;
            }
        } else if (set->lines[i].lru_clock <
                   set->lines[victim].lru_clock) {
            victim = i;
        }
    }

    return set->lines[victim].block_addr;
}

void replace_cacheline(const unsigned long long victim_block_addr,
                       const unsigned long long insert_addr,
                       Cache *cache) {
    Set *set = &cache->sets[cache_set(insert_addr, cache)];

    for (int i = 0; i < cache->linesPerSet; i++) {
        if (set->lines[i].valid &&
            set->lines[i].block_addr == victim_block_addr) {
            set->lines[i].tag = cache_tag(insert_addr, cache);
            set->lines[i].block_addr =
                address_to_block(insert_addr, cache);
            set->lines[i].lru_clock = set->lru_clock;
            set->lines[i].access_counter = 1;
            return;
        }
    }

    assert(0);
}

void cacheSetUp(Cache *cache, char *name) {
    cache->hit_count = 0;
    cache->miss_count = 0;
    cache->eviction_count = 0;

    cache->setBits = CACHE_SET_BITS;
    cache->linesPerSet = CACHE_LINES_PER_SET;
    cache->blockBits = CACHE_BLOCK_BITS;
    cache->displayTrace = CACHE_DISPLAY_TRACE;
    cache->lfu = CACHE_LFU;
    cache->name = name;

    unsigned long long num_sets = 1ULL << cache->setBits;

    cache->sets = calloc(num_sets, sizeof(Set));
    assert(cache->sets != NULL);

    for (unsigned long long i = 0; i < num_sets; i++) {
        cache->sets[i].lines =
            calloc(cache->linesPerSet, sizeof(Line));

        assert(cache->sets[i].lines != NULL);
    }
}

void deallocate(Cache *cache) {
    if (!cache->sets)
        return;

    unsigned long long num_sets = 1ULL << cache->setBits;

    for (unsigned long long i = 0; i < num_sets; i++)
        free(cache->sets[i].lines);

    free(cache->sets);
    cache->sets = NULL;
}

result operateCache(const unsigned long long address, Cache *cache) {
    result r = {0};

    Set *set = &cache->sets[cache_set(address, cache)];
    set->lru_clock++;

    if (probe_cache(address, cache)) {
        hit_cacheline(address, cache);
        cache->hit_count++;
        r.status = CACHE_HIT;

#ifdef PRINT_CACHE_TRACES
        printf(CACHE_HIT_FORMAT, address);
#endif

        return r;
    }

    cache->miss_count++;
    r.insert_block_addr = address_to_block(address, cache);

    if (insert_cacheline(address, cache)) {
        r.status = CACHE_MISS;

#ifdef PRINT_CACHE_TRACES
        printf(CACHE_MISS_FORMAT, address);
#endif

        return r;
    }

    r.status = CACHE_EVICT;
    r.victim_block_addr = victim_cacheline(address, cache);

    replace_cacheline(
        r.victim_block_addr,
        address,
        cache
    );

    cache->eviction_count++;

#ifdef PRINT_CACHE_TRACES
    printf(CACHE_EVICTION_FORMAT, address);
#endif

    return r;
}

int processCacheOperation(unsigned long address, Cache *cache) {
    result r = operateCache(
        (unsigned long long)(uint32_t)address,
        cache
    );

    if (r.status == CACHE_HIT) {
        hit_count++;
        return CACHE_HIT_LATENCY;
    }

    miss_count++;

    if (r.status == CACHE_MISS)
        return CACHE_MISS_LATENCY;

    return CACHE_OTHER_LATENCY;
}
