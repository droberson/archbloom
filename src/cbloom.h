/**
 * @file cbloom.h
 * @brief Header file for the counting Bloom filter implementation.
 * @author Daniel Roberson
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

#define CBLOOM_MAX_NAME_LENGTH 255

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
	uint64_t      expected; // TODO document
	float         accuracy; // TODO document
	char          name[CBLOOM_MAX_NAME_LENGTH + 1];
    counter_size  csize;  /**< Size of the counter (8, 16, 32, or 64 bits). */
    void         *countermap;  /**< Pointer to a map of element counters. */
} cbloomfilter;

// TODO document
typedef struct {
	uint8_t  magic[8];
	uint8_t  name[CBLOOM_MAX_NAME_LENGTH + 1];
	uint64_t size;
	uint64_t csize;
	uint64_t hashcount;
	uint64_t countermap_size;
	uint64_t expected;
	float    accuracy;
} cbloomfilter_file;

/* function declarations
 */
cbloom_error_t  cbloom_init(cbloomfilter *, const size_t, const float, counter_size);
void            cbloom_destroy(cbloomfilter *);
const char     *cbloom_get_name(cbloomfilter *); // TODO
bool            cbloom_set_name(cbloomfilter *, const char *); // TODO
size_t          cbloom_count(const cbloomfilter *, void *, size_t);
size_t          cbloom_count_string(const cbloomfilter *, char *);
bool            cbloom_lookup(const cbloomfilter *, void *, const size_t);
bool            cbloom_lookup_string(const cbloomfilter *, const char *);
void            cbloom_add(cbloomfilter *, void *, const size_t);
void            cbloom_add_string(cbloomfilter *, const char *);
float cbloom_get_average_count(cbloomfilter *); // TODO
size_t cbloom_count_elements_above_threshold(const cbloomfilter, size_t); // TODO
void            cbloom_remove(cbloomfilter *, void *, const size_t);
void            cbloom_remove_string(cbloomfilter *, const char *);
void            cbloom_clear(cbloomfilter *);
bool cbloom_clear_if_count_above(cbloomfilter *, const void *, size_t, size_t); // TODO
bool cbloom_clear_if_count_above_string(cbloomfilter *, const char *, size_t); // TODO
bool cbloom_clear_element(cbloomfilter *, const void *, size_t); // TODO
void cbloom_decay_linear(cbloomfilter *, size_t); // TODO
void cbloom_decay_exponential(cbloomfilter *, float); // TODO
uint64_t *cbloom_histogram(const cbloomfilter *); // TODO
size_t          cbloom_saturation_count(const cbloomfilter *);
float           cbloom_saturation(const cbloomfilter *);
cbloom_error_t  cbloom_save(cbloomfilter *, const char *); // TODO refactor
cbloom_error_t  cbloom_load(cbloomfilter *, const char *); // TODO refactor
cbloom_error_t cbloom_save_fd(cbloomfilter *, int); // TODO
cbloom_error_t cbloom_load_fd(cbloomfilter *, int); // TODO
const char     *cbloom_strerror(cbloom_error_t);

/*
 * TODO: 4 bit counters
 * TODO: name filters
 * TODO: file format struct, refactor cbloom_save/cbloom_load
 *
 * TODO: histograms. these can be used to detect anomalies and heavy
 * hitters. these can also be used by developers to determine if their
 * filters are working as they intend or to determine appropriate
 * rates to decay counters.
 *
 * TODO: count decaying. decrease by a fixed amount (linear), or
 * exponential (a percentage; this allows frequent elements to decay
 * slower than infrequent elements). this can be done at intervals
 * (hourly, daily, ...) or when a counter reaches a
 * threshold. decaying makes the filter prefer newer data and prevents
 * it from becoming oversaturated. consider adding mechanisms to do
 * this automatically.
 *
 * "In practice, when a counter does overflow, one approach is to
 * leave it at its maximum value. This can cause a later false
 * negative only if eventually the counter goes down to 0 when it
 * should have remained nonzero. If the deletions are random, the
 * expected time to this event is relatively large." - Broder &
 * Mitzenmacher
*/
#endif /* CBLOOM_H */
