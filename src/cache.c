#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hashtable.h"
#include "cache.h"

/**
 * Allocate a cache entry
 */
struct cache_entry *alloc_entry(char *path, char *content_type, void *content, int content_length)
{
    struct cache_entry *ce = malloc(sizeof(struct cache_entry));

    if (ce == NULL) {
        fprintf(stderr, "alloc_entry: malloc failed\n");
        return NULL;
    }

    ce->path         = strdup(path);
    ce->content_type = strdup(content_type);

    ce->content = malloc(content_length);
    if (ce->content == NULL) {
        fprintf(stderr, "alloc_entry: malloc for content failed\n");
        free(ce->path);
        free(ce->content_type);
        free(ce);
        return NULL;
    }
    memcpy(ce->content, content, content_length);
    ce->content_length = content_length;

    ce->prev = ce->next = NULL;

    return ce;
}

/**
 * Deallocate a cache entry
 */
void free_entry(struct cache_entry *entry)
{
    free(entry->path);
    free(entry->content_type);
    free(entry->content);
    free(entry);
}

/**
 * Insert a cache entry at the head of the linked list
 */
void dllist_insert_head(struct cache *cache, struct cache_entry *ce)
{
    // Insert at the head of the list
    if (cache->head == NULL) {
        cache->head = cache->tail = ce;
        ce->prev = ce->next = NULL;
    } else {
        cache->head->prev = ce;
        ce->next = cache->head;
        ce->prev = NULL;
        cache->head = ce;
    }
}

/**
 * Move a cache entry to the head of the list
 */
void dllist_move_to_head(struct cache *cache, struct cache_entry *ce)
{
    if (ce != cache->head) {
        if (ce == cache->tail) {
            // We're the tail
            cache->tail = ce->prev;
            cache->tail->next = NULL;

        } else {
            // We're neither the head nor the tail
            ce->prev->next = ce->next;
            ce->next->prev = ce->prev;
        }

        ce->next = cache->head;
        cache->head->prev = ce;
        ce->prev = NULL;
        cache->head = ce;
    }
}


/**
 * Removes the tail from the list and returns it
 *
 * NOTE: does not deallocate the tail
 */
struct cache_entry *dllist_remove_tail(struct cache *cache)
{
    struct cache_entry *oldtail = cache->tail;

    cache->tail = oldtail->prev;
    cache->tail->next = NULL;

    cache->cur_size--;

    return oldtail;
}

/**
 * Create a new cache
 *
 * max_size: maximum number of entries in the cache
 * hashsize: hashtable size (0 for default)
 */
struct cache *cache_create(int max_size, int hashsize)
{
    struct cache *cache = malloc(sizeof(struct cache));

    if (cache == NULL) {
        fprintf(stderr, "cache_create: malloc failed\n");
        return NULL;
    }

    cache->max_size = max_size;
    cache->cur_size = 0;
    cache->head     = NULL;
    cache->tail     = NULL;

    // Pass 0 or hashsize; NULL means use default hash function
    cache->index = hashtable_create(hashsize == 0 ? MAX_CACHE_ENTRIES * 2 : hashsize, NULL);

    if (cache->index == NULL) {
        fprintf(stderr, "cache_create: hashtable_create failed\n");
        free(cache);
        return NULL;
    }

    return cache;
}

void cache_free(struct cache *cache)
{
    struct cache_entry *cur_entry = cache->head;

    hashtable_destroy(cache->index);

    while (cur_entry != NULL) {
        struct cache_entry *next_entry = cur_entry->next;

        free_entry(cur_entry);

        cur_entry = next_entry;
    }

    free(cache);
}

/**
 * Store an entry in the cache
 *
 * This will also remove the least-recently-used items as necessary.
 *
 * NOTE: doesn't check for duplicate cache entries
 */
void cache_put(struct cache *cache, char *path, char *content_type, void *content, int content_length)
{
    // 1. Allocate the new entry
    struct cache_entry *ce = alloc_entry(path, content_type, content, content_length);
    if (ce == NULL) return;

    // 2. Evict LRU tail entries until there is room for the new one
    while (cache->cur_size >= cache->max_size) {
        struct cache_entry *evicted = dllist_remove_tail(cache);  // decrements cur_size
        hashtable_delete(cache->index, evicted->path);
        free_entry(evicted);
    }

    // 3. Insert new entry at head of the DLL (most-recently used)
    dllist_insert_head(cache, ce);
    cache->cur_size++;

    // 4. Add to the hashtable for O(1) future lookups
    hashtable_put(cache->index, ce->path, ce);
}

/**
 * Retrieve an entry from the cache
 */
struct cache_entry *cache_get(struct cache *cache, char *path)
{
    // Look up by path in the hashtable
    struct cache_entry *ce = hashtable_get(cache->index, path);

    if (ce == NULL) {
        return NULL;   // cache miss
    }

    // Cache hit — promote to head so it is the most-recently used
    dllist_move_to_head(cache, ce);

    return ce;
}