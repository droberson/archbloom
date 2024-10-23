/**
 * @file tdbloom.h
 * @brief Header file for the time-decaying Bloom filter implementation.
 *
 * This header file contains the declarations of functions and
 * structures for the time-decaying Bloom filter. The time-decaying
 * Bloom filter allows elements to expire after a specified timeout,
 * supporting operations such as initialization, element addition,
 * membership checks, expiration handling, and persistence
 * (saving/loading the filter).
 */
#ifndef TDBLOOM_H
#define TDBLOOM_H

#include <time.h>
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Error handling return values for time-decaying Bloom filter
 * operations.
 *
 * This enumeration defines the error codes that can be returned by
 * the time-decaying Bloom filter functions. Each error code
 * corresponds to a specific type of failure that might occur during
 * filter operations, such as memory allocation failure or file I/O
 * errors.
 */
typedef enum {
    TDBF_SUCCESS = 0,         /**< Operation completed successfully. */
    TDBF_INVALIDTIMEOUT,      /**< Timeout value is invalid or out of range. */
    TDBF_OUTOFMEMORY,         /**< Memory allocation failed. */
    TDBF_FOPEN,               /**< Failed to open file. */
    TDBF_FREAD,               /**< Failed to read from file. */
    TDBF_FWRITE,              /**< Failed to write to file. */
    TDBF_FSTAT,               /**< Failed to stat() the file descriptor. */
    TDBF_INVALIDFILE,         /**< Invalid or unparseable file format. */
    // Used for counting the number of statuses. Do not add statuses below this line.
    TDBF_ERRORCOUNT           /**< Total number of error statuses. */
} tdbloom_error_t;

/**
 * @brief Array of human-readable error messages corresponding to
 * `tdbloom_error_t` error codes.
 *
 * This array contains the string representations of error messages
 * that map to the error codes defined in `tdbloom_error_t`. These
 * messages provide a human-readable explanation of the error status
 * returned by time-decaying Bloom filter functions.
 */
const char *tdbloom_errors[] = {
    "Success",                 /**< TDBF_SUCCESS: Successfull operation. */
    "Invalid timeout value",   /**< TDBF_INVALIDTIMEOUT: Timeout value is invalid or out of range. */
    "Out of memory",           /**< TDBF_OUTOFMEMORY: Memory allocation failed. */
    "Unable to open file",     /**< TDBF_FOPEN: Failed to open file. */
    "Unable to read file",     /**< TDBF_FREAD: Failed to read from file. */
    "Unable to write to file", /**< TDBF_FWRITE: Failed to write to file. */
    "fstat() error",           /**< TDBF_FSTAT: Failed to stat() the file descriptor. */
    "Invalid file format"      /**< TDBF_INVALIDFILE: File format is invalid or unparseable. */
};

/**
 * @brief Structure for a time-decaying Bloom filter.
 *
 * This structure defines the components of a time-decaying Bloom
 * filter, which allows elements to naturally expire after a specified
 * timeout period. Each element is tracked with timestamps, and the
 * filter ensures that elements are only valid for a set number of
 * seconds.
 */
typedef struct {
    size_t  size;          /**< Size of the Bloom filter (number of bits). */
    size_t  hashcount;     /**< Number of hash functions applied per element. */
    size_t  timeout;       /**< Number of seconds an element remains valid before expiring. */
    size_t  filter_size;   /**< Size of the time filter (number of time_t values). */
    time_t  start_time;    /**< Timestamp when the filter was initialized. */
    size_t  expected;      /**< Expected number of elements to be stored in the filter. */
    float   accuracy;      /**< Desired false positive rate (e.g., 0.01 for 99.99% accuracy). */
    size_t  max_time;      /**< Maximum possible timestamp value in the filter. */
    int     bytes;         /**< Size of each timestamp in bytes. */
    void   *filter;        /**< Pointer to the array of time_t elements representing timestamps. */
} tdbloom;

/* function definitions
 */
tdbloom_error_t  tdbloom_init(tdbloom *,
                              const size_t,
                              const float,
                              const size_t);
void             tdbloom_destroy(tdbloom *);
void             tdbloom_clear(tdbloom *);
size_t           tdbloom_clear_expired(tdbloom *);
size_t           tdbloom_count_expired(tdbloom);
void             tdbloom_reset_start_time(tdbloom *);
float            tdbloom_saturation(const tdbloom);
void             tdbloom_add(tdbloom *, const void *, const size_t);
void             tdbloom_add_string(tdbloom, const char *);
bool             tdbloom_lookup(const tdbloom, const void *, const size_t);
bool             tdbloom_lookup_string(const tdbloom, const char *);
bool             tdbloom_has_expired(const tdbloom, const void *, size_t);
bool             tdbloom_has_expired_string(const tdbloom, const char *);
bool             tdbloom_reset_if_expired(tdbloom *, const void *, size_t);
bool             tdbloom_reset_if_expired_string(tdbloom *, const char *);

tdbloom_error_t  tdbloom_save(tdbloom, const char *);
tdbloom_error_t  tdbloom_load(tdbloom *, const char *);
const char      *tdbloom_strerror(tdbloom_error_t);

/*
 * TODO: potential additions
 * bool tdbloom_age_element(tdbloom *, const void *element, size_t len, size_t amount);
 * void tdbloom_adjust_timeout(tdbloom *, size_t new_timeout);
 * size_t tdbloom_saturation_count(const tdbloom);
 */
#endif /* TDBLOOM_H */
