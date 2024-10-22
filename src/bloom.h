/* bloom.h
 */
#ifndef BLOOM_H
#define BLOOM_H

#include <stdint.h>
#include <stdbool.h>

// TODO: implement 32, 64 bit functions? Need to test on a 32 bit system.
// #if UINTPTR_MAX == 0xffffffff
// typedef uint32_t (*hash_fun32_t)(const void *key, size_t len, uint32_t seed);
// #define DEFAULT_HASH_FUNC_BITS
// #else
// typedef uint64_t (*hash_fun64_t)(const void *key, size_t len, uint64_t seed);
// #endif /* UINTPTR_MAX */


/* bloom_error_t - error status type. used to map function return values to
 *                 error messages
 */
typedef enum {
	BF_SUCCESS = 0,
	BF_OUTOFMEMORY,
	BF_FOPEN,
	BF_FREAD,
	BF_FWRITE,
	BF_FSTAT,
	BF_INVALIDFILE,
	// ERRORCOUNT is used as a counter. do not add anything below this line.
	BF_ERRORCOUNT
} bloom_error_t;

const char *bloom_errors[] = {
	"Success",
	"Out of memory",
	"Unable to open file",
	"Unable to read file",
	"Unable to write to file",
	"fstat() failure",
	"Invalid file format"
};

/* bloomfilter -- typedef representing a bloom filter
 * TODO: specify hash function?
 */
typedef struct {
	size_t   size;              /* size of bloom filter */
	size_t   hashcount;         /* number of hashes per element */
	size_t   bitmap_size;       /* size of bitmap */
	size_t   expected;          /* expected capacity of filter*/
	size_t   insertions;        /* # of insertions into the filter */
	float    accuracy;          /* desired margin of error */
	uint8_t *bitmap;            /* bitmap of bloom filter */
} bloomfilter;

/* function declarations
 */
bloom_error_t  bloom_init(bloomfilter *, const size_t, const float);
void           bloom_destroy(bloomfilter *);
void           bloom_clear(bloomfilter *);
size_t         bloom_saturation_count(const bloomfilter);
float          bloom_saturation(const bloomfilter);
double         bloom_capacity(const bloomfilter);
bool           bloom_lookup(const bloomfilter, const void *, const size_t);
bool           bloom_lookup_string(const bloomfilter, const char *);
void           bloom_add(bloomfilter *, const void *, const size_t);
void           bloom_add_string(bloomfilter *, const char *);
bloom_error_t  bloom_save(const bloomfilter, const char *);
bloom_error_t  bloom_load(bloomfilter *, const char *);
const char    *bloom_strerror(const bloom_error_t);

#endif /* BLOOM_H */
