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
	COUNTER_4BIT,  /**< 4-bit counter, for very small element counts. (<=15) */
    COUNTER_8BIT,  /**< 8-bit counter, for small element counts. (<=255) */
    COUNTER_16BIT, /**< 16-bit counter, allows for moderate element counts. */
    COUNTER_32BIT, /**< 32-bit counter, suitable for larger element counts. */
    COUNTER_64BIT  /**< 64-bit counter, for very large element counts. */
} counter_size;

/**
 * @brief Structure for a counting Bloom filter.
 *
 * This structure defines the main components of a counting Bloom
 * filter. It allows for approximate membership testing with support
 * for adding and removing elements, while tracking the number of times
 * each element has been added. It’s designed to maintain a fixed
 * accuracy rate based on a set expected capacity.
 *
 * @var cbloomfilter::size
 * The total size of the counting Bloom filter in bits. This size
 * directly impacts the filter’s accuracy and capacity.
 *
 * @var cbloomfilter::hashcount
 * Number of hash functions applied to each element. Increasing the
 * number of hash functions generally reduces the false positive rate
 * but requires more computation for each operation.
 *
 * @var cbloomfilter::countermap_size
 * The total size of the counter map in bytes, which holds the counters
 * for tracking occurrences of elements in the filter.
 *
 * @var cbloomfilter::expected
 * Expected number of elements that the filter will hold. This parameter
 * helps determine the required size and counter settings to maintain
 * the desired accuracy rate.
 *
 * @var cbloomfilter::accuracy
 * Desired false positive rate, where values closer to 0 indicate a
 * lower rate of false positives. This parameter influences the filter’s
 * internal sizing and hash count calculations.
 *
 * @var cbloomfilter::name
 * A null-terminated character array used to identify the filter. This
 * can be useful for logging, debugging, or when managing multiple
 * filters.
 *
 * @var cbloomfilter::csize
 * Size of each counter used to track element occurrences, which can be
 * set to 8, 16, 32, or 64 bits based on expected element counts.
 * Smaller counters reduce memory usage but may overflow if elements
 * are added frequently.
 *
 * @var cbloomfilter::countermap
 * Pointer to the memory map containing counters for each element. Each
 * counter represents the count of hash mappings for an element in the
 * filter.
 */
typedef struct {
    uint64_t      size; /**< Size of the counting Bloom filter. */
    uint64_t      hashcount; /**< Number of hash functions used per element. */
    uint64_t      countermap_size; /**< Total size of the counter map. */
	uint64_t      expected; /**< Expected number of elements in the filter. */
	float         accuracy; /**< Desired false positive rate (e.g., 0.01 for 1% false positive rate). */
	char          name[CBLOOM_MAX_NAME_LENGTH + 1]; /**< Null-terminated name of the filter. */
    counter_size  csize;  /**< Size of the counter (8, 16, 32, or 64 bits). */
    void         *countermap;  /**< Pointer to a map of element counters. */
} cbloomfilter;

/**
 * @brief Structure representing metadata for saving/loading a counting Bloom filter.
 *
 * This structure is used to store essential metadata about a counting Bloom
 * filter, facilitating saving and loading the filter’s state to and from disk.
 * It includes identifiers, filter parameters, and configuration details required
 * to reconstruct the filter in memory.
 *
 * @var cbloomfilter_file::magic
 * An 8-byte magic identifier string (e.g., "!cbloom!") that verifies the file
 * format, ensuring that the file is a valid counting Bloom filter file.
 *
 * @var cbloomfilter_file::name
 * A null-terminated character array identifying the filter, limited to
 * CBLOOM_MAX_NAME_LENGTH characters.
 *
 * @var cbloomfilter_file::size
 * Total size of the counting Bloom filter in bits, representing the filter’s
 * overall capacity.
 *
 * @var cbloomfilter_file::csize
 * Size of each counter in bits (e.g., 8, 16, 32, or 64 bits). Determines the
 * maximum count each entry can track.
 *
 * @var cbloomfilter_file::hashcount
 * Number of hash functions applied to each element, influencing both accuracy
 * and computational complexity.
 *
 * @var cbloomfilter_file::countermap_size
 * Size of the counter map in bytes, reflecting the total memory allocated for
 * storing element counts.
 *
 * @var cbloomfilter_file::expected
 * Expected number of elements the filter is designed to hold, used in
 * calculating the required filter size and hash count.
 *
 * @var cbloomfilter_file::accuracy
 * Desired false positive rate for the filter, guiding the internal sizing and
 * configuration parameters.
 */
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
const char     *cbloom_get_name(cbloomfilter *);
bool            cbloom_set_name(cbloomfilter *, const char *);
cbloom_error_t  cbloom_save(cbloomfilter *, const char *);
cbloom_error_t  cbloom_load(cbloomfilter *, const char *);
cbloom_error_t  cbloom_save_fd(cbloomfilter *, int);
cbloom_error_t  cbloom_load_fd(cbloomfilter *, int);

size_t          cbloom_count(const cbloomfilter *, void *, size_t);
size_t          cbloom_count_string(const cbloomfilter *, char *);
float           cbloom_get_average_count(cbloomfilter *);
size_t          cbloom_count_elements_above_threshold(const cbloomfilter *,
                                                      uint64_t);
size_t          cbloom_saturation_count(const cbloomfilter *);
float           cbloom_saturation(const cbloomfilter *);

bool            cbloom_lookup(const cbloomfilter *, void *, const size_t);
bool            cbloom_lookup_string(const cbloomfilter *, const char *);
bool            cbloom_lookup_or_add(cbloomfilter *, void *, const size_t);
bool            cbloom_lookup_or_add_string(cbloomfilter *, const char *);

void            cbloom_add(cbloomfilter *, void *, const size_t);
void            cbloom_add_string(cbloomfilter *, const char *);
// TODO are these necessary?
bool            cbloom_add_if_not_present(cbloomfilter *, void *, const size_t);
bool            cbloom_add_if_not_present_string(cbloomfilter *, const char *);

void            cbloom_remove(cbloomfilter *, void *, const size_t);
void            cbloom_remove_string(cbloomfilter *, const char *);
void            cbloom_clear(cbloomfilter *);
bool            cbloom_clear_element(cbloomfilter *, void *, size_t);
bool            cbloom_clear_element_string(cbloomfilter *, const char *);
bool            cbloom_clear_if_count_above(cbloomfilter *,
                                            const void *,
                                            size_t,
                                            size_t);
bool            cbloom_clear_if_count_above_string(cbloomfilter *,
                                                   const char *,
                                                   size_t);
void            cbloom_decay_linear(cbloomfilter *, size_t);
void            cbloom_decay_exponential(cbloomfilter *, float);

//uint64_t *cbloom_histogram(const cbloomfilter *); // TODO

const char     *cbloom_strerror(cbloom_error_t);

/*
 * TODO: histograms. these can be used to detect anomalies and heavy
 * hitters. these can also be used by developers to determine if their
 * filters are working as they intend or to determine appropriate
 * rates to decay counters.
 *
*/
#endif /* CBLOOM_H */
