/**
 * @file tdcbloom.h
 * @brief Header file for time-decaying, counting Bloom filter implementation.
 * @author Daniel Roberson
 *
 * This header file contains the declarations of functions and
 * structures for a time-decaying, counting Bloom filter. This type of
 * filter tracks both the frequency of elements and their recency,
 * allowing for time-based expiration of elements as well as counting
 * occurrences within a specified time window. The file provides
 * support for operations such as initialization, adding elements,
 * checking membership and expiration, and persistence (saving/loading
 * the filter).
 */
#ifndef TDCBLOOM_H
#define TDCBLOOM_H

#include <time.h>
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief tdcbloom_error_t
 */
typedef enum {
	TDCBF_SUCCESS = 0,
	TDCBF_OUTOFMEMORY,
	TDCBF_INVALIDCOUNTERSIZE,
	TDCBF_INVALIDTIMERSIZE,
	TDCBF_INVALIDEXPECTED,
	TDCBF_INVALIDACCURACY,
	// counter
	TDCBF_ERRORCOUNT
} tdcbloom_error_t;

/**
 * @brief tdcbloom_errors - Human-readable error messages.
 */
const char *tdcbloom_errors[] = {
	"Success",
	"Out of memory",
	"Invalid counter size",
	"Invalid timer size",
	"Invalid number of expected elements",
	"Invalid accuracy parameter"
};

/**
 * @brief counter_size bit size of counter items
 */
typedef enum {
	COUNTER_8BIT,
	COUNTER_16BIT,
	COUNTER_32BIT,
	COUNTER_64BIT
} counter_size;

/**
 * @brief timer_size bit size of timer elements
 */
typedef enum {
	TIMER_8BIT,
	TIMER_16BIT,
	TIMER_32BIT,
	TIMER_64BIT
} timer_size;

/**
 * @brief tdcbloom_entry - structure containing timestamp and counter
 */
typedef struct {
	void *counter;
	void *timestamp;
} tdcbloom_entry;

/**
 * @brief tdcbloom - Time-decaying, counting Bloom filter structure.
 */
typedef struct {
	uint64_t        size; /* size */
	time_t          start_time;
	size_t          timeout;
	size_t          max_time;
	uint64_t        hashcount;
	counter_size    counter_size;
	int             counter_size_bytes;
	timer_size      timer_size;
	int             timer_size_bytes;
	size_t          entry_size; // entry size = counter size + timer size
	tdcbloom_entry *entrymap;
} tdcbloom;

/* function definitions
 */
tdcbloom_error_t  tdcbloom_init(tdcbloom *,
                                const size_t,
                                const float,
                                const size_t,
                                counter_size,
                                timer_size);
void              tdcbloom_destroy(tdcbloom *);
void              tdcbloom_clear(tdcbloom *);
size_t            tdcbloom_count(const tdcbloom *, const void *, const size_t);
size_t            tdcbloom_count_string(const tdcbloom *, const char *);
size_t            tdcbloom_clear_expired(tdcbloom *);
size_t            tdcbloom_count_expired(tdcbloom *);
void              tdcbloom_reset_start_time(tdcbloom *);
float             tdcbloom_saturation(const tdcbloom *);
size_t            tdcbloom_saturation_count(const tdcbloom *);

void              tdcbloom_add(tdcbloom *, const void *, const size_t);
void              tdcbloom_add_string(tdcbloom *, const char *);
bool              tdcbloom_lookup(const tdcbloom *, const void *, const size_t);
bool              tdcbloom_lookup_string(const tdcbloom *, const char *);
void              tdcbloom_remove(tdcbloom *, const void *, const size_t);
void              tdcbloom_remove_string(tdcbloom *, const char *);

tdcbloom_error_t  tdcbloom_save(const tdcbloom *, const char *);
tdcbloom_error_t  tdcbloom_load(tdcbloom *, const char *);
const char       *tdcbloom_strerror(tdcbloom_error_t);


bool              tdcbloom_has_expired(const tdcbloom *, const void *, size_t);
bool              tdcbloom_has_expired_string(const tdcbloom *, const char *);
bool              tdcbloom_reset_if_expired(tdcbloom *, const void *, size_t);
bool              tdcbloom_reset_if_expired_string(tdcbloom *, const char *);
bool              tdcbloom_age_element(tdcbloom *, const void *, size_t, size_t);
void              tdcbloom_adjust_timeout(tdcbloom *, size_t);
float             tdcbloom_get_average_count(const tdcbloom *);
size_t            tdcbloom_age_and_remove(tdcbloom *, size_t);

#endif /* TDCBLOOM_H */
