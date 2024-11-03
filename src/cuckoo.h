/* cuckoo.h
 */
#ifndef CUCKOO_H
#define CUCKOO_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

/* cuckoobucket -- typedef for cuckoo filter bucket
 */
typedef struct {
	uint32_t fingerprint;
} cuckoobucket;

/* cuckoofilter -- cuckoo filter structure
 */
typedef struct {
	cuckoobucket *buckets;
	size_t        num_buckets;
	size_t        bucket_size;       /* 2, 4, or 8 */
	size_t        max_kicks;
	size_t        total_insertions;  /* insertion counter */
	size_t       *bucket_insertions; /* insertion counters per bucket */
	size_t        evictions;         /* eviction counter */
	uint32_t      prng_state;        /* xorshift state */
} cuckoofilter;

/* function definitions
 */
bool   cuckoo_init(cuckoofilter *, size_t, size_t, size_t);
void   cuckoo_destroy(cuckoofilter *);
bool   cuckoo_add(cuckoofilter, void *, size_t);
bool   cuckoo_add_string(cuckoofilter, char *);
bool   cuckoo_lookup(cuckoofilter, void *, size_t);
bool   cuckoo_lookup_string(cuckoofilter, char *);
bool   cuckoo_remove(cuckoofilter, void *, size_t);
bool   cuckoo_remove_string(cuckoofilter, char *);
double cuckoo_load_factor(cuckoofilter);
bool   cuckoo_save(cuckoofilter, const char *);
bool   cuckoo_load(cuckoofilter *, const char *);

#endif /* CUCKOO_H */
