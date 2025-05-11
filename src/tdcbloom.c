/**
 * @file tdcbloom.c
 * @brief Time-decaying, counting Bloom filters.
 * @author Daniel Roberson
 *
 * This is a time-decaying, counting Bloom filter implementation.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <time.h>
#include <math.h>
#include <sys/stat.h>

#include "tdcbloom.h"
#include "mmh3.h"

/**
 * @brief Calculate the ideal size of a Bloom filter's bit array.
 *
 * This function calculates the optimal size of a Bloom filter's bit array
 * based on the expected number of elements that it will contain and the
 * desired accuracy. This ensures a good balance of memory usage and acceptable
 * rate of false positive results
 *
 * @param expected Maximum expected number of elements to store in the filter.
 * @param accuracy The desired rate of false positives (eg 0,01 for
 * 99.99% accuracy).
 *
 * @return The optimal size of the filter based on given inputs.
 *
 * @note This function is static and intended for internal use.
 */
static uint64_t ideal_size(const uint64_t expected, const float accuracy) {
	return -(expected * log(accuracy) / pow(log(2.0), 2));
}

/**
 * @brief Retrieves the current monotonic time.
 *
 * This function returns the current time based on the monotonic
 * clock, which is not affected by system clock changes (e.g.,
 * daylight savings or manual clock adjustments). Using monotonic time
 * ensures that the filter remains consistent even if the system clock
 * changes.
 *
 * @return A `time_t` value representing the current monotonic time
 * (CLOCK_MONOTONIC).
 *
 * @note This function is static and intended for internal use.
 */
static time_t get_monotonic_time() {
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);

	return ts.tv_sec;
}

/**
 * @brief Initializes a time-decaying counting Bloom filter.
 *
 * @param tdcbf Pointer to the tdcbloom structure to initialize.
 * @param expected Maximum expected number of elements to store.
 * @param accuracy Desired false positive rate (e.g., 0.01 for 99.99% accuracy).
 * @param timeout Number of seconds an element remains valid before expiring.
 * @param countersize Size of the counter (e.g., COUNTER_8BIT, COUNTER_16BIT, etc.).
 * @param timersize Size of the timer (e.g., TIME_8BIT, TIME_16BIT, etc.).
 *
 * @return TDCBF_SUCCESS on success.
 * @return TDCBF_OUTOFMEMORY if out of memory.
 */
tdcbloom_error_t tdcbloom_init(tdcbloom *tdcbf,
							   const size_t expected,
							   const float accuracy,
							   const size_t timeout,
							   counter_size countersize,
							   timer_size timersize) {
	if (expected == 0) {
		return TDCBF_INVALIDEXPECTED;
	}

	if (accuracy <= 0.0f || accuracy >= 1.0f) {
		return TDCBF_INVALIDACCURACY;
	}

	tdcbf->size          = ideal_size(expected, accuracy);
	tdcbf->hashcount     = (tdcbf->size / expected) * log(2);
	tdcbf->timeout       = timeout;
	tdcbf->start_time    = get_monotonic_time();
	tdcbf->counter_size  = countersize;
	tdcbf->timer_size    = timersize;

	switch (countersize) {
	case COUNTER_8BIT:  tdcbf->counter_size_bytes = sizeof(uint8_t);  break;
	case COUNTER_16BIT: tdcbf->counter_size_bytes = sizeof(uint16_t); break;
	case COUNTER_32BIT: tdcbf->counter_size_bytes = sizeof(uint32_t); break;
	case COUNTER_64BIT: tdcbf->counter_size_bytes = sizeof(uint64_t); break;
	default: return TDCBF_INVALIDCOUNTERSIZE;
	}

	switch (timersize) {
	case TIMER_8BIT:  tdcbf->timer_size_bytes = sizeof(uint8_t);  break;
	case TIMER_16BIT: tdcbf->timer_size_bytes = sizeof(uint16_t); break;
	case TIMER_32BIT: tdcbf->timer_size_bytes = sizeof(uint32_t); break;
	case TIMER_64BIT: tdcbf->timer_size_bytes = sizeof(uint64_t); break;
	default: return TDCBF_INVALIDTIMERSIZE;
	}

	tdcbf->entry_size = tdcbf->counter_size_bytes + tdcbf->timer_size_bytes;

	tdcbf->max_time = \
		(timersize == TIMER_8BIT)  ? UINT8_MAX  :
		(timersize == TIMER_16BIT) ? UINT16_MAX :
		(timersize == TIMER_32BIT) ? UINT32_MAX : UINT64_MAX;

	tdcbf->entrymap = calloc(tdcbf->size, tdcbf->entry_size);
	if (tdcbf->entrymap == NULL) {
		return TDCBF_OUTOFMEMORY;
	}

	return TDCBF_SUCCESS;
}

/**
 * @brief Destroy a time-decaying counting Bloom filter.
 *
 * Frees any memory allocated by the tdcbloom filter.
 *
 * @param tdcbf Pointer to the tdcbloom structure to destroy.
 */
void tdcbloom_destroy(tdcbloom *tdcbf) {
	if (tdcbf->entrymap) {
		free(tdcbf->entrymap);
		tdcbf->entrymap = NULL;
	}
}

/**
 * @brief Clear the contents of the time-decaying counting Bloom filter.
 *
 * This function resets all counters and timestamps in the filter to
 * zero, effectively clearing the filter.
 *
 * The start time is also reset to the current time.
 *
 * @param tdcbf Pointer to time-decaying counting Bloom filter to clear.
 *
 * TODO: test
 */
void tdcbloom_clear(tdcbloom *tdcbf) {
	memset(tdcbf->entrymap, 0, tdcbf->size * tdcbf->entry_size);
	tdcbf->start_time = get_monotonic_time();
}

// helper function to read timer
static inline uint64_t read_timer(void *timestamp, timer_size tsize) {
	switch (tsize) {
	case TIMER_8BIT:  return *(uint8_t *)timestamp;
	case TIMER_16BIT: return *(uint16_t *)timestamp;
	case TIMER_32BIT: return *(uint32_t *)timestamp;
	case TIMER_64BIT: return *(uint64_t *)timestamp;
	}
	return 0; // Default return if size is not handled
}

// helper function to write timer
static inline void write_timer(void *timestamp, timer_size tsize, uint64_t value) {
	switch (tsize) {
	case TIMER_8BIT:  *(uint8_t *)timestamp = value; break;
	case TIMER_16BIT: *(uint16_t *)timestamp = value; break;
	case TIMER_32BIT: *(uint32_t *)timestamp = value; break;
	case TIMER_64BIT: *(uint64_t *)timestamp = value; break;
	}
}

// helper function to read counter
static inline uint64_t read_counter(void *counter, counter_size csize) {
	switch (csize) {
	case COUNTER_8BIT:  return *(uint8_t *)counter;
	case COUNTER_16BIT: return *(uint16_t *)counter;
	case COUNTER_32BIT: return *(uint32_t *)counter;
	case COUNTER_64BIT: return *(uint64_t *)counter;
	}
	return 0;
}

// helper function to write counter
static inline void write_counter(void *counter, counter_size csize, uint64_t value) {
	switch (csize) {
	case COUNTER_8BIT:  *(uint8_t *)counter = value; break;
	case COUNTER_16BIT: *(uint16_t *)counter = value; break;
	case COUNTER_32BIT: *(uint32_t *)counter = value; break;
	case COUNTER_64BIT: *(uint64_t *)counter = value; break;
	}
}

/**
 * @brief Remove expired entries from a time-decaying counting Bloom
 * filter and return the number of removed entries.
 *
 * This function iterates over the entire filter, clearing entries
 * where the timestamp has exceeded the configured timeout.
 *
 * @param tdcbf Pointer to the time-decaying counting Bloom filter.
 *
 * @return The number of expired entries removed.
 *
 * TODO: test
 */
size_t tdcbloom_clear_expired(tdcbloom *tdcbf) {
	time_t now           = get_monotonic_time();
	size_t expired_count = 0;

	for (size_t i = 0; i < tdcbf->size; i++) {
		uint8_t *entry = (uint8_t *)tdcbf->entrymap + (i * tdcbf->entry_size);
		void *counter = entry;
		void *timestamp = entry + (tdcbf->entry_size - tdcbf->timer_size_bytes);
		uint64_t entry_ts = read_timer(timestamp, tdcbf->timer_size);

		// Check if the entry has expired
		if (entry_ts != 0 && (now - entry_ts) > tdcbf->timeout) {
			memset(counter, 0, tdcbf->counter_size_bytes);
			memset(timestamp, 0, tdcbf->timer_size_bytes);
			expired_count++;
		}
	}

	return expired_count;
}

/**
 * @brief Count expired entries from a time-decaying counting Bloom
 * filter and return the number of expired entries.
 *
 * This function iterates over the entire filter, counting entries
 * where the timestamp has exceeded the configured timeout.
 *
 * @param tdcbf Pointer to the time-decaying counting Bloom filter.
 *
 * @return The number of expired entries removed.
 *
 * TODO: Test
 */
size_t tdcbloom_count_expired(tdcbloom *tdcbf) {
	time_t now           = get_monotonic_time();
	size_t expired_count = 0;

	for (size_t i = 0; i < tdcbf->size; i++) {
		uint8_t *entry = (uint8_t *)tdcbf->entrymap + (i * tdcbf->entry_size);
		void *counter = entry;
		void *timestamp = entry + (tdcbf->entry_size - tdcbf->timer_size_bytes);
		uint64_t entry_ts = read_timer(timestamp, tdcbf->timer_size);

		// Check if the entry has expired
		if (entry_ts != 0 && (now - entry_ts) > tdcbf->timeout) {
			expired_count++;
		}
	}

	return expired_count;
}

/**
 * @brief Reset the start time of the time-decaying counting Bloom filter.
 *
 * This function resets the start time to the current time, allowing
 * the time-decay logic to restart without clearing the data in the
 * filter. This could be useful for "pausing" the decay or extending
 * the time horizon.
 *
 * @param tdcbf Pointer to the time-decaying counting Bloom filter.
 *
 * TODO: test
 */
void tdcbloom_reset_start_time(tdcbloom *tdcbf) {
	tdcbf->start_time = get_monotonic_time();
}

/**
 * @brief Adjust the timeout value of a time-decaying counting Bloom filter.
 *
 * This function adjusts the timeout value for the filter and clears
 * any elements that have already expired under the new timeout
 * setting. The timeout specifies how long elements remain valid in
 * the filter, so changing it could result in the expiration of
 * previously valid entries.
 *
 * @param tdcbf Pointer to the time-decaying counting Bloom filter.
 * @param new_timeout The new timeout value in seconds.
 *
 * @note This function recalculates expiration based on the new
 * timeout and removes elements that have exceeded this time limit. It
 * can be relatively expensive for large filters, as it involves
 * scanning and adjusting each entry.
 *
 * TODO: lazy expiration
 * TODO: test
 */
void  tdcbloom_adjust_timeout(tdcbloom *tdcbf, size_t new_timeout) {
	time_t now = get_monotonic_time();
	tdcbf->timeout = new_timeout;

	for (size_t i = 0; i < tdcbf->size; i++) {
		uint8_t *entry = (uint8_t *)tdcbf->entrymap + (i * tdcbf->entry_size);
		void *timestamp_ptr = entry + tdcbf->counter_size_bytes;
		uint64_t timestamp = read_timer(timestamp_ptr, tdcbf->timer_size);

		if (timestamp != 0) { // adjust timeout to account for new timeout
			time_t elapsed_time = (now - timestamp + tdcbf->max_time) % tdcbf->max_time;

			if (elapsed_time > new_timeout) { // clear expired entries
				memset(entry, 0, tdcbf->entry_size);
			}
		}
	}
}

/**
 * @brief Calculate the saturation of the time-decaying counting Bloom filter.
 *
 * This function computes the saturation (percentage of active
 * entries) of the time-decaying counting Bloom filter by counting how
 * many entries are non-zero and dividing by the total size.
 *
 * @param tdcbf The time-decaying counting Bloom filter to calculate
 * saturation for.
 *
 * @return The saturation percentage (0.0 to 100.0) of the filter.
 *
 * TODO: test
 */
float tdcbloom_saturation(const tdcbloom *tdcbf) {
	size_t active_count = tdcbloom_saturation_count(tdcbf);
	return (float)active_count / (float)tdcbf->size * 100.0;
}

/**
 * @brief Count the number of active entries in the time-decaying
 * counting Bloom filter.
 *
 * This function iterates over the filter and counts how many entries
 * (counters and timestamps) are non-zero, which indicates how many
 * items have been added to the filter that have not expired.
 *
 * @param tdcbf Pointer to the time-decaying counting Bloom filter.
 *
 * @return The number of active entries (non-zero counter and timestamp).
 *
 * TODO: test
 */
size_t tdcbloom_saturation_count(const tdcbloom *tdcbf) {
	size_t count = 0;

	for (size_t i = 0; i < tdcbf->size; i++) {
		uint8_t *entry = (uint8_t *)tdcbf->entrymap + (i * tdcbf->entry_size);
		void *counter = entry;
		void *timestamp = entry + (tdcbf->entry_size - tdcbf->timer_size_bytes);
		bool is_non_zero = false;

		switch (tdcbf->counter_size) {
		case COUNTER_8BIT:
			if (*(uint8_t *)counter != 0) {
				is_non_zero = true;
			}
			break;
		case COUNTER_16BIT:
			if (*(uint16_t *)counter != 0) {
				is_non_zero = true;
			}
			break;
		case COUNTER_32BIT:
			if (*(uint32_t *)counter != 0) {
				is_non_zero = true;
			}
			break;
		case COUNTER_64BIT:
			if (*(uint64_t *)counter != 0) {
				is_non_zero = true;
			}
			break;
		}

		if (is_non_zero == false) { // count is zero. check timestamps.
			switch (tdcbf->timer_size) {
			case TIMER_8BIT:
				if (*(uint8_t *)timestamp  != 0) {
					count++;
				}
				break;
			case TIMER_16BIT:
				if (*(uint16_t *)timestamp != 0) {
					count++;
				}
				break;
			case TIMER_32BIT:
				if (*(uint32_t *)timestamp != 0) {
					count++;
				}
				break;
			case TIMER_64BIT:
				if (*(uint64_t *)timestamp != 0) {
					count++;
				}
				break;
			}
		} else {
			count++; // counter is non-zero.
		}
	}

	return count;
}

/**
 * @brief Increment the counter at the given pointer, with bounds checking.
 *
 * @param counter Pointer to the counter to increment.
 * @param csize Size of the counter (COUNTER_8BIT, COUNTER_16BIT, etc.).
 *
 * @note This function is static and intended for internal use.
 */
static inline void increment_counter(void *counter, counter_size csize) {
	switch (csize) {
	case COUNTER_8BIT:
		if (*(uint8_t *)counter < UINT8_MAX) {
			(*(uint8_t *)counter)++;
		}
		break;
	case COUNTER_16BIT:
		if (*(uint16_t *)counter < UINT16_MAX) {
			(*(uint16_t *)counter)++;
		}
		break;
	case COUNTER_32BIT:
		if (*(uint32_t *)counter < UINT32_MAX) {
			(*(uint32_t *)counter)++;
		}
		break;
	case COUNTER_64BIT:
		if (*(uint64_t *)counter < UINT64_MAX) {
			(*(uint64_t *)counter)++;
		}
		break;
	}
}

/**
 * @brief Decrement the counter at the given pointer, with bounds checking.
 *
 * @param counter Pointer to the counter to decrement.
 * @param csize Size of the counter (COUNTER_8BIT, COUNTER_16BIT, etc.).
 *
 * @note This function is static and intended for internal use.
 */
static inline void decrement_counter(void *counter, counter_size csize) {
	switch (csize) {
	case COUNTER_8BIT:
		if (*(uint8_t *)counter > 0) {
			(*(uint8_t *)counter)--;
		}
		break;
	case COUNTER_16BIT:
		if (*(uint16_t *)counter > 0) {
			(*(uint16_t *)counter)--;
		}
		break;
	case COUNTER_32BIT:
		if (*(uint32_t *)counter > 0) {
			(*(uint32_t *)counter)--;
		}
		break;
	case COUNTER_64BIT:
		if (*(uint64_t *)counter > 0) {
			(*(uint64_t *)counter)--;
		}
		break;
	}
}

// set_timestamp() - helper function to set a timestamp
static inline void set_timestamp(void *timestamp, timer_size tsize, time_t ts) {
	switch (tsize) {
	case TIMER_8BIT:
		(*(uint8_t *)timestamp) = (uint8_t)(ts % UINT8_MAX);
		break;
	case TIMER_16BIT:
		(*(uint16_t *)timestamp) = (uint16_t)(ts % UINT16_MAX);
		break;
	case TIMER_32BIT:
		(*(uint32_t *)timestamp) = (uint32_t)(ts % UINT32_MAX);
		break;
	case TIMER_64BIT:
		(*(uint64_t *)timestamp) = (uint64_t)(ts % UINT64_MAX);
		break;
	}
}

/**
 * @brief Get the average count of elements in the time-decaying
 * counting Bloom filter.
 *
 * This function calculates the average count of all non-zero entries
 * in the time-decaying counting Bloom filter. It iterates over all
 * entries and updates the average incrementally to avoid overflow.
 *
 * @param tdcbf The time-decaying counting Bloom filter.
 *
 * @return The average count of non-zero entries in the filter. If
 * there are no non-zero entries, the function returns 0.0.
 */
float tdcbloom_get_average_count(const tdcbloom *tdcbf) {
	size_t non_zero_entries = 0;
	double average_count    = 0.0;

	for (size_t i = 0; i < tdcbf->size; i++) {
		uint8_t *entry = (uint8_t *)tdcbf->entrymap + (i * tdcbf->entry_size);
		uint64_t count = read_counter(entry, tdcbf->counter_size);

		if (count > 0) {
			non_zero_entries++;
			average_count += (double)(count - average_count) / non_zero_entries;
		}
	}

	return (non_zero_entries == 0) ? 0.0 : (float)average_count;
}

/**
 * @brief Add an element to the time-decaying counting Bloom filter.
 *
 * This function inserts an element into the time-decaying counting
 * Bloom filter.  It increments the counter at each of the hashed
 * positions and updates the timestamp for each position.
 *
 * @param tdcbf Pointer to the time-decaying counting Bloom filter.
 * @param element Pointer to the element to add to the filter.
 * @param len Length of the element in bytes.
 */
void tdcbloom_add(tdcbloom *tdcbf, const void *element, const size_t len) {
	uint64_t position;
	time_t   now = get_monotonic_time();
	uint64_t hashes[tdcbf->hashcount];

	mmh3_64_make_hashes(element, len, tdcbf->hashcount, hashes);

	for (size_t i = 0; i < tdcbf->hashcount; i++) {
		position = hashes[i] % tdcbf->size;
		uint8_t *entry = (uint8_t *)tdcbf->entrymap + (position * tdcbf->entry_size);
		increment_counter(entry, tdcbf->counter_size);
		void *timestamp_base = entry + tdcbf->counter_size_bytes;
		set_timestamp(timestamp_base, tdcbf->timer_size, now);
	}
}

/**
 * @brief Add a string element to the time-decaying counting Bloom filter.
 *
 * This is a convenience function for adding string elements.
 *
 * @param tdcbf Pointer to the time-decaying counting Bloom filter.
 * @param element Pointer to the string element to add.
 */
void tdcbloom_add_string(tdcbloom *tdcbf, const char *element) {
	tdcbloom_add(tdcbf, element, strlen(element));
}

/**
 * @brief Lookup an element in the time-decaying counting Bloom filter.
 *
 * This function checks if an element is likely present in the filter
 * and has not expired.
 *
 * @param tdcbf Pointer to the time-decaying counting Bloom filter.
 * @param element Pointer to the element to look up.
 * @param len Length of the element in bytes.
 *
 * @return true if the element is likely in the filter and has not expired.
 * @return false if the element is definitely not in the filter or has expired.
 */
bool tdcbloom_lookup(const tdcbloom *tdcbf, const void *element, const size_t len) {
	uint64_t position;
	time_t now = get_monotonic_time();
	uint64_t hashes[tdcbf->hashcount];

	mmh3_64_make_hashes(element, len, tdcbf->hashcount, hashes);

	for (size_t i = 0; i < tdcbf->hashcount; i++) {
		position = hashes[i] % tdcbf->size;

		uint8_t *entry = (uint8_t *)tdcbf->entrymap + (position * tdcbf->entry_size);
		uint64_t counter = read_counter(entry, tdcbf->counter_size);

		if (counter == 0) {
			return false; // definitely not in the filter
		}

		void *timestamp_ptr = entry + tdcbf->counter_size_bytes;
		uint64_t timestamp = read_timer(timestamp_ptr, tdcbf->timer_size);

		if (((now - timestamp + tdcbf->max_time) % tdcbf->max_time) > tdcbf->timeout) {
			return false; // timed out
		}
	}

	return true; // element likely exists and isn't expired
}

/**
 * @brief Lookup a string element in the time-decaying counting Bloom filter.
 *
 * This is a convenience function for looking up string elements.
 *
 * @param tdcbf Pointer to the time-decaying counting Bloom filter.
 * @param element Pointer to the string element to lookup.
 *
 * @return true if the element is likely in the filter and has not expired.
 * @return false if the element is definitely not in the filter or has expired.
 */
bool tdcbloom_lookup_string(const tdcbloom *tdcbf, const char *element) {
	return tdcbloom_lookup(tdcbf, element, strlen(element));
}

/**
 * @brief Check if an element has expired in the time-decaying
 * counting Bloom filter.
 *
 * This function checks whether the given element has expired in the
 * filter, meaning it was once present but its validity period has
 * elapsed.
 *
 * @param tdcbf Pointer to the time-decaying counting Bloom filter.
 * @param element Pointer to the element to check for expiration.
 * @param len Length of the element in bytes.
 *
 * @return true if the element has expired
 * @return false if the element is still valid or never existed.
 */
bool tdcbloom_has_expired(const tdcbloom *tdcbf, const void *element, size_t len) {
	uint64_t result;
	uint64_t hashes[tdcbf->hashcount];
	time_t now = get_monotonic_time();

	mmh3_64_make_hashes(element, len, tdcbf->hashcount, hashes);

	for (size_t i = 0; i < tdcbf->hashcount; i++) {
		result = hashes[i] % tdcbf->size;

		uint8_t *entry = (uint8_t *)tdcbf->entrymap + (result * tdcbf->entry_size);
		uint64_t counter = read_counter(entry, tdcbf->counter_size);

		if (counter == 0) {
			return false; // Element is not in the filter
		}

		void *timestamp_ptr = entry + tdcbf->counter_size_bytes;
		uint64_t timestamp = read_timer(timestamp_ptr, tdcbf->timer_size);

		if ((now - timestamp + tdcbf->max_time) % tdcbf->max_time > tdcbf->timeout) {
			return true; // Element has expired
		}
	}

	return false; // Element is still valid
}

// TODO test
bool tdcbloom_has_expired_string(const tdcbloom *tdcbf, const char *element) {
	return tdcbloom_has_expired(tdcbf, element, strlen(element));
}

/**
 * @brief Check if an element has expired in the time-decaying
 * counting Bloom filter, and reset it if it has expired.
 *
 * This function checks whether the given element has expired in the
 * time-decaying counting Bloom filter. If the element has expired
 * (i.e., its timestamp exceeds the configured timeout), the function
 * will reset its timestamp and counter, effectively refreshing the
 * element's time-to-live.
 *
 * @param tdcbf Pointer to the time-decaying counting Bloom filter.
 * @param element Pointer to the element to check and possibly reset.
 * @param len Length of the element in bytes.
 *
 * @return true if the element was expired and has been reset.
 * @return false if the element was still valid or did not exist.
 *
 * TODO test
 */
bool tdcbloom_reset_if_expired(tdcbloom *tdcbf, const void *element, size_t len) {
	if (tdcbloom_has_expired(tdcbf, element, len)) {
		tdcbloom_add(tdcbf, element, len);
		return true; // element was expired and has been reset
	}
	return false; // element still valid or did not exist
}

/**
 * @brief Check if a string element has expired in the time-decaying
 * counting Bloom filter, and reset it if it has expired.
 *
 * This function checks whether the given string element has expired
 * in the time-decaying counting Bloom filter. If the element has
 * expired (i.e., its timestamp exceeds the configured timeout), the
 * function will reset its timestamp and counter, effectively
 * refreshing the element's time-to-live.
 *
 * @param tdcbf Pointer to the time-decaying counting Bloom filter.
 * @param element Pointer to the string element to check and possibly reset.
 *
 * @return true if the string element was expired and has been reset.
 * @return false if the string element was still valid or did not exist.
 *
 * TODO test
 */
bool tdcbloom_reset_if_expired_string(tdcbloom *tdcbf, const char *element) {
	return tdcbloom_reset_if_expired(tdcbf, element, strlen(element));
}

/**
 * @brief Remove an element from the time-decaying counting Bloom filter.
 *
 * This function decrements the counter of the hashed positions for an element.
 * If the counter is already zero, it remains unchanged.
 *
 * @param tdcbf Pointer to the time-decaying counting Bloom filter.
 * @param element Pointer to the element to remove.
 * @param len Length of the element in bytes.
 *
 * TODO should this return a value? true if removed, false if not removed?
 */
void tdcbloom_remove(tdcbloom *tdcbf, const void *element, const size_t len) {
	uint64_t position;
	uint64_t hashes[tdcbf->hashcount];

	mmh3_64_make_hashes(element, len, tdcbf->hashcount, hashes);

	for (size_t i = 0; i < tdcbf->hashcount; i++) {
		position = hashes[i] % tdcbf->size;
		uint8_t *entry = (uint8_t *)tdcbf->entrymap + (position * tdcbf->entry_size);
		decrement_counter(entry, tdcbf->counter_size);
	}
}

/**
 * @brief Remove a string element from the time-decaying counting Bloom filter.
 *
 * This is a convenience function for removing string elements.
 *
 * @param tdcbf Pointer to the time-decaying counting Bloom filter.
 * @param element Pointer to the string element to remove.
 */
void tdcbloom_remove_string(tdcbloom *tdcbf, const char *element) {
	tdcbloom_remove(tdcbf, element, strlen(element));
}

/**
 * @brief Count the occurrences of an element in the time-decaying
 * counting Bloom filter.
 *
 * This function returns the approximate count of the specified
 * element by summing the values of the counters at the hashed
 * positions.
 *
 * @param tdcbf The time-decaying counting Bloom filter.
 * @param element Pointer to the element to count.
 * @param len Length of the element in bytes.
 *
 * @return The approximate count of the element in the filter.
 */
size_t tdcbloom_count(const tdcbloom *tdcbf, const void *element, const size_t len) {
	uint64_t position;
	size_t   total_count = SIZE_MAX;
	time_t   now = get_monotonic_time();
	uint64_t hashes[tdcbf->hashcount];

	mmh3_64_make_hashes(element, len, tdcbf->hashcount, hashes);

	for (size_t i = 0; i < tdcbf->hashcount; i++) {
		position = hashes[i] % tdcbf->size;

		uint8_t *entry = (uint8_t *)tdcbf->entrymap + (position * tdcbf->entry_size);
		uint64_t counter = read_counter(entry, tdcbf->counter_size);

		if (counter == 0) {
			return 0;  // element is definitely not in set
		}

		void *timestamp_ptr = entry + tdcbf->counter_size_bytes;
		uint64_t timestamp = read_timer(timestamp_ptr, tdcbf->timer_size);

		if ((now - timestamp) % tdcbf->max_time > tdcbf->timeout) {
			return 0;  // element has expired
		}

		if (counter < total_count) {
			total_count = counter;
		}
	}

	return total_count;
}

/**
 * @brief Count the occurrences of a string element in the
 * time-decaying counting Bloom filter.
 *
 * This function returns the approximate count of the specified string
 * element by calling the `tdcbloom_count()` function.
 *
 * @param tdcbf The time-decaying counting Bloom filter.
 * @param element Pointer to the string element to count.
 *
 * @return The approximate count of the string element in the filter.
 */
size_t tdcbloom_count_string(const tdcbloom *tdcbf, const char *element) {
	return tdcbloom_count(tdcbf, element, strlen(element));
}

/**
 * @brief Age an element in the time-decaying counting Bloom filter.
 *
 * This function manually ages an element in the time-decaying
 * counting Bloom filter by updating its timestamp to reflect the
 * current time. This may be useful for scenarios where you want to
 * effectively shorten the life of an element, without waiting for it
 * to expire on its own.
 *
 * @param tdcbf Pointer to the time-decaying counting Bloom filter.
 * @param element Pointer to the element to age.
 * @param len Length of the element in bytes.
 * @param age_amount Length in second to age an element.
 *
 * @return true if the element was found and aged.
 * @return false if the element was not found in the filter.
 *
 * TODO: test
 */
bool tdcbloom_age_element(tdcbloom *tdcbf, const void *element, size_t len, size_t age_amount) {
	uint64_t result;
	uint64_t hashes[tdcbf->hashcount];
	time_t now = get_monotonic_time();

	mmh3_64_make_hashes(element, len, tdcbf->hashcount, hashes);

	for (size_t i = 0; i < tdcbf->hashcount; i++) {
		result = hashes[i] % tdcbf->size;

		uint8_t *entry = (uint8_t *)tdcbf->entrymap + (result * tdcbf->entry_size);
		uint64_t counter = read_counter(entry, tdcbf->counter_size);

		if (counter == 0) {
			return false; // element definitely not in the filter
		}

		void *timestamp_ptr = entry + tdcbf->counter_size_bytes;
		uint64_t timestamp = read_timer(timestamp_ptr, tdcbf->timer_size);

		if (timestamp > age_amount) {
			timestamp -= age_amount;
		} else {
			timestamp = 0; // expired. reset timer
		}

		write_timer(timestamp_ptr, tdcbf->timer_size, timestamp);
	}

	return true; // element was found and aged successfully
}

/**
 * @brief Age and remove expired elements from the time-decaying
 * counting Bloom filter.
 *
 * This function iterates through the Bloom filter, aging and removing
 * elements that have a timestamp older than the specified
 * `max_age`. It effectively purges elements that have been in the
 * filter longer than the maximum allowed age, freeing up space in the
 * filter.
 *
 * @param tdcbf Pointer to the time-decaying counting Bloom filter.
 * @param max_age Maximum allowed age (in seconds) for elements.
 * Elements older than this will be removed.
 *
 * @return The number of elements removed from the filter.
 *
 * TODO test
 */
size_t tdcbloom_age_and_remove(tdcbloom *tdcbf, size_t max_age) {
	time_t now = get_monotonic_time();
	size_t removed_count = 0;

	for (size_t i = 0; i < tdcbf->size; i++) {
		uint8_t *entry = (uint8_t *)tdcbf->entrymap + (i * tdcbf->entry_size);
		uint64_t counter = read_counter(entry, tdcbf->counter_size);

		if (counter == 0) {
			continue; // skip if counter is zero
		}

		void *timestamp_ptr = entry + tdcbf->counter_size_bytes;
		uint64_t timestamp = read_timer(timestamp_ptr, tdcbf->timer_size);
		time_t element_age = (now >= timestamp) ? (now - timestamp) : (now + (tdcbf->max_time - timestamp));

		if (element_age > max_age) {
			// Clear the counter and the timestamp to "remove" the element
			memset(entry, 0, tdcbf->counter_size_bytes);
			memset(timestamp_ptr, 0, tdcbf->timer_size_bytes);
			removed_count++;
		}
	}

	return removed_count;
}

/**
 * TODO: implement tdcbloom_save()
 */
tdcbloom_error_t tdcbloom_save(const tdcbloom *tdcbf, const char *path) {
	return TDCBF_SUCCESS;
}

/**
 * TODO: implement tdcbloom_load()
 */
tdcbloom_error_t tdcbloom_load(tdcbloom *tdcbf, const char *path) {
	return TDCBF_SUCCESS;
}

/**
 * @brief Return a string containing the error message for a given error code.
 *
 * This function converts an error code returned by a time-decaying,
 * counting Bloom filter function into a human-readable error message.
 *
 * @param error The error code returned by a time-decaying counting
 * Bloom filter function.
 *
 * @return A pointer to a string containing the relevant error
 * message, or "Unknown error" if the error code is out of range.
 *
 * TODO: test
 */
const char *tdcbloom_strerror(tdcbloom_error_t error) {
	if (error < 0 || error >= TDCBF_ERRORCOUNT) {
		return "Unknown error";
	}

	return tdcbloom_errors[error];
}
