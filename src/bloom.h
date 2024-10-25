/**
 * @file bloom.h
 * @brief Header file for Bloom filter implementation
 * @author Daniel Roberson
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
 * @var bloomfilter::accuracy
 * Desired margin of error (e.g., 0.01 represents 99.99% accuracy).
 *
 * @var bloomfilter::bitmap
 * Pointer to the bitmap used to represent the Bloom filter.
 */
typedef struct {
	size_t   size;              /**< Size of the Bloom filter in bits */
	size_t   hashcount;         /**< Number of hashes performed per element */
	size_t   bitmap_size;       /**< Size of the bitmap in bytes */
	size_t   expected;          /**< Expected capacity of the filter */
	float    accuracy;          /**< Desired margin of error */
	uint8_t *bitmap;            /**< Pointer to the bitmap of the filter */
} bloomfilter;


/**
 * @struct bloomfilter_file
 * @brief Structure representing metadata for saving/loading a Bloom filter.
 *
 * This structure is used to store essential metadata about a Bloom
 * filter, such as its size, the number of hash functions, and the
 * desired accuracy.  It facilitates saving and loading a Bloom
 * filter's state to/from a file.
 *
 * @var bloomfilter_file::size
 * Total number of bits in the Bloom filter's bit array.
 *
 * @var bloomfilter_file::hashcount
 * Number of hash functions used in the Bloom filter.
 *
 * @var bloomfilter_file::bitmap_size
 * Size of the bit array in bytes.
 *
 * @var bloomfilter_file::expected
 * Expected number of elements the Bloom filter is configured to hold.
 *
 * @var bloomfilter_file::accuracy
 * Desired false positive rate, where 0.01 represents 99% accuracy.
 */
typedef struct {
	uint8_t  magic[8];
	uint8_t  name[256];
	uint64_t size;
	uint64_t hashcount;
	uint64_t bitmap_size;
	uint64_t expected;
	float    accuracy;
} bloomfilter_file;

/* function declarations
 */
bloom_error_t  bloom_init(bloomfilter *, const size_t, const float);
void           bloom_destroy(bloomfilter *);
void           bloom_clear(bloomfilter *);
const char    *bloom_strerror(const bloom_error_t);
bloom_error_t  bloom_save(const bloomfilter *, const char *);
bloom_error_t  bloom_load(bloomfilter *, const char *);
bloom_error_t  bloom_save_fd(const bloomfilter *, int);
bloom_error_t  bloom_load_fd(bloomfilter *, int);
bloom_error_t  bloom_merge(bloomfilter *,
						   const bloomfilter *,
						   const bloomfilter *);
bloom_error_t  bloom_intersect(bloomfilter *,
							   const bloomfilter *,
							   const bloomfilter *);
float          bloom_estimate_intersection(const bloomfilter *,
										   const bloomfilter *);
size_t         bloom_saturation_count(const bloomfilter *);
float          bloom_saturation(const bloomfilter *);
bool           bloom_clear_if_saturation_exceeds(bloomfilter *,
												 float threshold);
float          bloom_estimate_false_positive_rate(const bloomfilter *);

bool           bloom_lookup(const bloomfilter *, const void *, const size_t);
bool           bloom_lookup_string(const bloomfilter *, const char *);
bool           bloom_lookup_or_add(bloomfilter *, const void *, const size_t);
bool           bloom_lookup_or_add_string(bloomfilter *, const char *);

void           bloom_add(bloomfilter *, const void *, const size_t);
void           bloom_add_string(bloomfilter *, const char *);
bool           bloom_add_if_not_present(bloomfilter *,
										const void *,
										const size_t);
bool           bloom_add_if_not_present_string(bloomfilter *, const char *);

#endif /* BLOOM_H */

/* https://www.eecs.harvard.edu/~michaelm/postscripts/im2005b.pdf
 *
 * TODO: Another nice feature is that Bloom filters can easily be
 * halved in size, allowing an application to dynamically shrink a
 * Bloom filter. Suppose that the size of the filter is a power of
 * 2. To halve the size of the filter, just OR the first and second
 * halves together. When hashing to do a lookup, the highest order bit
 * can be masked.
 *
 * TODO: compressed bloom filters - by making the filters larger
 * initally, then using compression, the filters can take up even less
 * space than a "properly sized" Bloom filter - Broder & Mitzenmacher
 *
 * TODO: specify hash function. this currently uses mmh3 128 bit,
 * spliting the resulting hash into two 64 bit hashes, and using
 * double hashing for the remaining hashes. It may be useful to be
 * able to switch hash functions.
 */
