/**
 * @file cbloom.h
 * @brief Header file for the counting Bloom filter implementation.
 *
 * This file contains the declarations for the counting Bloom filter
 * functions and data structures. The counting Bloom filter allows
 * approximate membership checking with support for element addition,
 * removal, and counting the occurrences of elements.
 *
 * The file defines the `cbloomfilter` structure and related functions
 * for initializing, adding elements, removing elements, and checking
 * membership, as well as error handling types and functions.
 */
#ifndef CBLOOM_H
#define CBLOOM_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Error status type used for mapping function return values to
 * error messages.
 *
 * This enumeration defines the possible error codes that can be
 * returned by the counting Bloom filter functions. Each error code
 * corresponds to a specific type of failure that might occur during
 * filter operations, such as memory allocation failure or file I/O
 * errors.
 */
typedef enum {
	CBF_SUCCESS = 0,        /**< Operation completed successfully. */
    CBF_OUTOFMEMORY,        /**< Memory allocation failed. */
    CBF_INVALIDCOUNTERSIZE, /**< Invalid counter size specified. */
    CBF_FOPEN,              /**< Failed to open file. */
    CBF_FWRITE,             /**< Failed to write to file. */
    CBF_FREAD,              /**< Failed to read from file. */
    CBF_FSTAT,              /**< Failed to stat() the file descriptor. */
    CBF_INVALIDFILE,        /**< Invalid or unparseable file format. */
    // dummy enum to use as a counter. do not add entries after CBF_ERRORCOUNT.
    CBF_ERRORCOUNT          /**< Total number of error types. */
} cbloom_error_t;

const char *cbloom_errors[] = {
	"Success",
	"Out of memory",
	"Invalid counter size",
	"Unable to open file",
	"Unable to write to file",
	"Unable to read file",
	"fstat() failure",
	"Invalid file format"
};

/**
 * @brief Enum for selecting the size of counters in the counting Bloom filter.
 *
 * This enumeration defines the available sizes for the counters used
 * in a counting Bloom filter. By selecting a smaller counter size,
 * the memory footprint can be reduced when fewer counts are expected.
 * Larger counters should be used when higher counts are anticipated.
 */
typedef enum {
    COUNTER_8BIT,  /**< 8-bit counter, suitable for small element counts. */
    COUNTER_16BIT, /**< 16-bit counter, allows for moderate element counts. */
    COUNTER_32BIT, /**< 32-bit counter, suitable for larger element counts. */
    COUNTER_64BIT  /**< 64-bit counter, for very large element counts. */
} counter_size;

/**
 * @brief Structure for a counting Bloom filter.
 *
 * This structure defines the main components of a counting Bloom
 * filter. It allows for approximate membership testing, with support
 * for adding and removing elements. Each element is tracked by a
 * counter to count the number of times it has been added.
 */
typedef struct {
    uint64_t      size; /**< Size of the counting Bloom filter in bits. */
    uint64_t      hashcount; /**< Number of hash functions used per element. */
    uint64_t      countermap_size; /**< Total size of the counter map. */
    counter_size  csize;  /**< Size of the counter (8, 16, 32, or 64 bits). */
    void         *countermap;  /**< Pointer to a map of element counters. */
} cbloomfilter;

/* function declarations
 */
cbloom_error_t  cbloom_init(cbloomfilter *, const size_t, const float, counter_size);
void            cbloom_destroy(cbloomfilter *);
size_t          cbloom_count(const cbloomfilter, void *, size_t);
size_t          cbloom_count_string(const cbloomfilter, char *);
bool            cbloom_lookup(const cbloomfilter, void *, const size_t);
bool            cbloom_lookup_string(const cbloomfilter, const char *);
void            cbloom_add(cbloomfilter, void *, const size_t);
void            cbloom_add_string(cbloomfilter, const char *);
void            cbloom_remove(cbloomfilter, void *, const size_t);
void            cbloom_remove_string(cbloomfilter, const char *);
void            cbloom_clear(cbloomfilter *);
size_t          cbloom_saturation_count(const cbloomfilter);
float           cbloom_saturation(const cbloomfilter);
cbloom_error_t  cbloom_save(cbloomfilter, const char *);
cbloom_error_t  cbloom_load(cbloomfilter *, const char *);
const char     *cbloom_strerror(cbloom_error_t);

#endif /* CBLOOM_H */
