/**
 * @file bloom.h
 * @brief Header file for Bloom filter implementation
 *
 * This file contains the function declarations, type definitions, and
 * macros for working with Bloom filters. It provides an interface for
 * initializing, adding elements, checking for the existence of
 * elements, and saving/loading Bloom filters to and from disk.
 *
 * It also defines error handling mechanisms and utilities for managing
 * the filter's memory and accuracy parameters.
 *
 * @see bloom.c for the corresponding implementation.
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


/**
 * @enum bloom_error_t
 * @brief Enum representing error status codes for Bloom filter operations.
 *
 * This enum defines various error codes that can be returned by Bloom
 * filter functions. Each error code maps to a corresponding error
 * message in the `bloom_errors[]` array.
 *
 * @var BF_SUCCESS
 * Indicates that the operation completed successfully.
 *
 * @var BF_OUTOFMEMORY
 * Error indicating memory allocation failure.
 *
 * @var BF_FOPEN
 * Error indicating failure to open a file.
 *
 * @var BF_FREAD
 * Error indicating failure to read from a file.
 *
 * @var BF_FWRITE
 * Error indicating failure to write to a file.
 *
 * @var BF_FSTAT
 * Error indicating failure of the fstat() system call.
 *
 * @var BF_INVALIDFILE
 * Error indicating that the file format is invalid.
 *
 * @var BF_ERRORCOUNT
 * A counter used internally to track the number of error codes. No
 * new errors should be added below this line.
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

/**
 * @var bloom_errors
 * @brief Array of error messages that correspond to Bloom filter errors codes.
 *
 * This array houses human-readable error messages that correspond to
 * the error codes returned by Bloom filter functions. Each index in
 * the array matches a specific error code.
 *
 * @note The order of the messages must align with their corresponding
 * error codes.
 */
const char *bloom_errors[] = {
	"Success",
	"Out of memory",
	"Unable to open file",
	"Unable to read file",
	"Unable to write to file",
	"fstat() failure",
	"Invalid file format"
};

/**
 * @struct bloomfilter
 * @brief Bloom filter data structure.
 *
 * This struct defines the core components of a bloom filter. This
 * includes size of the filter, number of hashes to perform, and the
 * bitmap that stores data relating to elements residing within the
 * filter.
 *
 * @var bloomfilter::size
 * Size of the Bloom filter in bits.
 *
 * @var bloomfilter::hashcount
 * Number of hash functions applied per element.
 *
 * @var bloomfilter::bitmap_size
 * Size of the bitmap in bytes.
 *
 * @var bloomfilter::expected
 * Expected number of elements the filter will hold.
 *
 * @var bloomfilter::insertions
 * Number of elements inserted into the filter.
 *
 * @var bloomfilter::accuracy
 * Desired margin of error (e.g., 0.01 represents 99.99% accuracy).
 *
 * @var bloomfilter::bitmap
 * Pointer to the bitmap used to represent the Bloom filter.
 *
 * TODO: specify hash function?
 */
typedef struct {
	size_t   size;              /**< Size of the Bloom filter in bits */
	size_t   hashcount;         /**< Number of hashes performed per element */
	size_t   bitmap_size;       /**< Size of the bitmap in bytes */
	size_t   expected;          /**< Expected capacity of the filter */
	size_t   insertions;        /**< Insertions counter */
	float    accuracy;          /**< Desired margin of error */
	uint8_t *bitmap;            /**< Pointer to the bitmap of the filter */
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
bool           bloom_lookup_or_add(bloomfilter *, const void *, const size_t);
bool           bloom_lookup_or_add_string(bloomfilter *, const char *);
void           bloom_add(bloomfilter *, const void *, const size_t);
void           bloom_add_string(bloomfilter *, const char *);
bloom_error_t  bloom_save(const bloomfilter, const char *);
bloom_error_t  bloom_load(bloomfilter *, const char *);
const char    *bloom_strerror(const bloom_error_t);

#endif /* BLOOM_H */
