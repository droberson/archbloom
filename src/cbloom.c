/**
 * @file cbloom.c
 * @brief Implementation of a counting Bloom filter.
 * @author Daniel Roberson
 *
 * This file contains the implementation of a counting Bloom filter,
 * which allows for approximate membership checks and supports element
 * addition and removal using counters. It includes functions for
 * initializing, adding elements, checking membership, removing
 * elements, and saving/loading the filter to/from disk.
 */
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <stdio.h>
#include <sys/stat.h>

#include "mmh3.h"
#include "cbloom.h"

/**
 * @brief Calculate the ideal size of a counting Bloom filter.
 *
 * This function calculates the optimal size of a counting Bloom
 * filter's bit array based on the expected number of elements that it
 * will contain and the desired accuracy. This ensures a good balance
 * of memory usage and acceptable rate of false positive results
 *
 * @param expected Maximum expected number of elements to store in the filter.
 * @param accuracy Desired rate of false positives (eg 0.01 for 99.99% accuracy)
 *
 * @return The optimal size of the filter, in number of elements. The
 * size of the elements is determined by the size of the counters.
 *
 * @note This function is static and intended for internal use.
 *
 * TODO: move this to its own file because its used in every other
 *       bloom filter implementation in this collection. perhaps use
 *       __attribute__((visibility("hidden"))) and compile with
 *       -fvisibility=hidden?
 * TODO: overflow check?
 */
static uint64_t ideal_size(const uint64_t expected, const float accuracy) {
	return -(expected * log(accuracy) / pow(log(2.0), 2));
}

/**
 * @brief Initialize a counting Bloom filter.
 *
 * This function sets up a counting Bloom filter based on the expected
 * number of elements, desired accuracy, and the specified counter
 * size. The counter size can be chosen based on the expected number
 * of insertions, allowing for optimized memory usage.
 *
 * @param cbf Pointer to the counting Bloom filter to initialize.
 * @param expected Expected number of elements to store in the filter.
 * @param accuracy Desired false positive rate (e.g., 0.01 for 99.99% accuracy).
 * @param csize Size of the counter, which can be one of COUNTER_8BIT,
 * COUNTER_16BIT, COUNTER_32BIT, or COUNTER_64BIT.
 *
 * @return CBF_SUCCESS on success.
 * @return CBF_OUTOFMEMORY if memory allocation fails.
 * @return CBF_INVALIDSIZE if the counter size is invalid.
 * @return CBF_INVALIDPARAM if one or more input parameters are invalid.
 * @return CBF_ERROR if an unspecified error occurs.
 */
cbloom_error_t cbloom_init(cbloomfilter *cbf, const size_t expected, const float accuracy, counter_size csize) {
	cbf->size      = ideal_size(expected, accuracy);
	// add 0.5 to round up/down
	cbf->hashcount = (uint64_t)((cbf->size / expected) * log(2) + 0.5);
	cbf->csize     = csize;
	cbf->accuracy  = accuracy;
	cbf->expected  = expected;
	strncpy(cbf->name, "DEFAULT", 7);

	switch (csize) {
	case COUNTER_4BIT:
		cbf->countermap_size = (cbf->size + 1) / 2;
		break;
	case COUNTER_8BIT:
		cbf->countermap_size = cbf->size * sizeof(uint8_t);
		break;
	case COUNTER_16BIT:
		cbf->countermap_size = cbf->size * sizeof(uint16_t);
		break;
	case COUNTER_32BIT:
		cbf->countermap_size = cbf->size * sizeof(uint32_t);
		break;
	case COUNTER_64BIT:
		cbf->countermap_size = cbf->size * sizeof(uint64_t);
		break;
	default: // invalid counter size
		return CBF_INVALIDCOUNTERSIZE;
	}

	cbf->countermap = calloc(1, cbf->countermap_size);
	if (cbf->countermap == NULL) {
		return CBF_OUTOFMEMORY;
	}

	return CBF_SUCCESS;
}

/**
 * @brief Frees memory allocated by `cbloom_init()`.
 *
 * This function releases all resources and memory allocated for the
 * counting Bloom filter during its initialization. After calling this
 * function, the filter should not be used unless reinitialized.
 *
 * @param cbf Pointer to the counting Bloom filter to free.
 */
void cbloom_destroy(cbloomfilter *cbf) {
	if (cbf->countermap) {
		free(cbf->countermap);
		cbf->countermap = NULL;
	}
}

/**
 * @brief Set the name of the counting Bloom filter.
 *
 * This function sets a name for the counting Bloom filter. The name is
 * useful for identifying filters when managing multiple instances.
 *
 * @param cbf Pointer to the counting Bloom filter.
 * @param name Null-terminated string to be set as the filter's name.
 *             If the string length exceeds `CBLOOM_MAX_NAME_LENGTH`,
 *             the function returns `false` without setting the name.
 *
 * @return `true` if the name was successfully set.
 * @return `false` if the name length exceeds `CBLOOM_MAX_NAME_LENGTH`.
 *
 * TODO: test
 */
bool cbloom_set_name(cbloomfilter *cbf, const char *name) {
	if (strlen(name) > CBLOOM_MAX_NAME_LENGTH) {
		return false;
	}

	strncpy(cbf->name, name, CBLOOM_MAX_NAME_LENGTH);
	cbf->name[CBLOOM_MAX_NAME_LENGTH] = '\0';

	return true;
}

/**
 * @brief Retrieve the name of the counting Bloom filter.
 *
 * This function returns the name assigned to the counting Bloom filter.
 * The name can help with identification in applications where multiple
 * filters are in use.
 *
 * @param cbf Pointer to the counting Bloom filter.
 *
 * @return Pointer to the null-terminated name of the filter.
 *
 * TODO: test
 */
const char *cbloom_get_name(cbloomfilter *cbf) {
	return cbf->name;
}

/* get_counter, set_counter, inc_counter_amount, dec_counter_amount,
 *     inc_counter, dec_counter -- helper functions used to handle
 *     different sized counters.
 */
static inline uint64_t get_counter(const cbloomfilter *cbf, uint64_t position) {
	switch (cbf->csize) {
	case COUNTER_4BIT:
		uint8_t byte = ((uint8_t *)cbf->countermap)[position / 2];
		return (position % 2 == 0) ? (byte & 0x0f) : (byte >> 4);
	case COUNTER_8BIT:	return  ((uint8_t *)cbf->countermap)[position];
	case COUNTER_16BIT:	return ((uint16_t *)cbf->countermap)[position];
	case COUNTER_32BIT:	return ((uint32_t *)cbf->countermap)[position];
	case COUNTER_64BIT:	return ((uint64_t *)cbf->countermap)[position];
	default:
		return 0; // shouldn't get here
	}
}

static inline void set_counter(const cbloomfilter *cbf, uint64_t position, uint64_t value) {
	switch (cbf->csize) {
	case COUNTER_4BIT:
		uint8_t *byte = &((uint8_t *)cbf->countermap)[position / 2];
		value = (value > 15) ? 15 : value; // 4 bit max is 15
		if (position % 2 == 0) {
			*byte = (*byte & 0xf0) | value; // lower nibble
		} else {
			*byte = (*byte & 0x0f) | (value << 4); // upper nibble
		}
		break;
	case COUNTER_8BIT:
		((uint8_t *)cbf->countermap)[position] =
			(value > UINT8_MAX) ? UINT8_MAX : (uint8_t)value;
		break;
	case COUNTER_16BIT:
		((uint16_t *)cbf->countermap)[position] =
			(value > UINT16_MAX) ? UINT16_MAX : (uint16_t)value;
		break;
	case COUNTER_32BIT:
		((uint32_t *)cbf->countermap)[position] =
			(value > UINT32_MAX) ? UINT32_MAX : (uint32_t)value;
		break;
	case COUNTER_64BIT:
		((uint64_t *)cbf->countermap)[position] = value; // size check unneeded
		break;
	default:
		return; // shouldn't get here
	}
}

static void inc_counter_amount(cbloomfilter *cbf, uint64_t position, uint64_t amount) {
	uint64_t counter_value = get_counter(cbf, position);
	set_counter(cbf, position, counter_value + amount);
}

static void inc_counter(cbloomfilter *cbf, uint64_t position) {
	inc_counter_amount(cbf, position, 1);
}

static void dec_counter_amount(cbloomfilter *cbf, uint64_t position, uint64_t amount) {
	uint64_t counter_value = get_counter(cbf, position);

	if (counter_value <= amount) {
		set_counter(cbf, position, 0);
	} else {
		set_counter(cbf, position, counter_value - amount);
	}
}

static void dec_counter(cbloomfilter *cbf, uint64_t position) {
	dec_counter_amount(cbf, position, 1);
}

/**
 * @brief Retrieve the approximate count of an element in the counting
 * Bloom filter.
 *
 * This function returns the approximate count of the specified
 * element in the counting Bloom filter. Since the Bloom filter is
 * probabilistic in nature, the count may not be exact but provides a
 * reasonable estimate.
 *
 * @param cbf Counting Bloom filter to use.
 * @param element Pointer to the element to count.
 * @param len Length of the element in bytes.
 *
 * @return A `size_t` value representing the approximate count of the
 * element in the filter.
 */
size_t cbloom_count(const cbloomfilter *cbf, void *element, size_t len) {
	uint64_t hashes[cbf->hashcount];
	uint64_t position;
	uint64_t count = UINT64_MAX;

	mmh3_64_make_hashes(element, len, cbf->hashcount, hashes);

	for (int i = 0; i < cbf->hashcount; i++) {
		position = hashes[i] % cbf->size;

		uint64_t current_count = get_counter(cbf, position);
		if (current_count < count) {
			count = current_count;
		}
	}

	return count;
}

/**
 * @brief Helper function to retrieve the approximate count of a
 * string element in the counting Bloom filter.
 *
 * This function returns the approximate count of the specified string
 * element in the counting Bloom filter. Since Bloom filters are
 * probabilistic, the count is not exact but provides a reasonable
 * estimate.
 *
 * @param cbf Counting Bloom filter to use.
 * @param element Pointer to the string element to count.
 *
 * @return A `size_t` value representing the approximate count of the
 * string element in the filter.
 */
size_t cbloom_count_string(const cbloomfilter *cbf, char *element) {
	return cbloom_count(cbf, (uint8_t *)element, strlen(element));
}

/**
 * @brief Estimate the number of unique elements with counters above a
 * specified threshold.
 *
 * This function iterates over the counters in the counting Bloom
 * filter, counting the number of counters that exceed a specified
 * threshold. The count is then divided by `hashcount` to provide an
 * estimate of the number of unique elements that likely exceed this
 * threshold.
 *
 * @param cbf Pointer to the counting Bloom filter.
 * @param threshold The counter value threshold to compare against.
 * @return An approximate count of unique elements with counters above
 *         the threshold.
 */
size_t cbloom_count_elements_above_threshold(const cbloomfilter *cbf, uint64_t threshold) {
    size_t count        = 0;

    for (size_t i = 0; i < cbf->size; i++) {
        uint64_t counter_value = get_counter(cbf, i);
        if (counter_value > threshold) {
            count++;
        }
    }

    return count / cbf->hashcount;
}

/**
 * @brief Calculate the average count of non-zero counters in the
 * counting Bloom filter.
 *
 * This function computes the average count across all non-zero
 * counters in the counting Bloom filter, giving an indication of the
 * filter's general usage level. Zeroed counters are discarded to
 * avoid skewing the average towards zero.
 *
 * @param cbf Pointer to the counting Bloom filter.
 *
 * @return The average count as a floating-point number. If no
 *         counters are set, returns 0.0.
 */
float cbloom_get_average_count(cbloomfilter *cbf) {
    uint64_t total_count       = 0;
    size_t   non_zero_counters = 0;

    for (size_t i = 0; i < cbf->size; i++) {
        uint64_t counter_value = get_counter(cbf, i);
        if (counter_value > 0) {
            total_count += counter_value;
            non_zero_counters++;
        }
    }

    if (non_zero_counters == 0) {
        return 0.0;
    }

    return (float)total_count / non_zero_counters;
}

/**
 * @brief Check if an element is likely in the counting Bloom filter.
 *
 * This function checks whether the specified element is likely
 * present in the counting Bloom filter. Due to the probabilistic
 * nature of Bloom filters, false positives are possible. If the
 * function returns `true`, the element is probably in the filter. If
 * it returns `false`, the element is definitely not in the filter.
 *
 * @param cbf Counting Bloom filter to use.
 * @param element Pointer to the element to look up.
 * @param len Length of the element in bytes.
 *
 * @return `true` if the element is likely in the filter.
 * @return `false` if the element is definitely not in the filter.
 */
bool cbloom_lookup(const cbloomfilter *cbf, void *element, const size_t len) {
	uint64_t hashes[cbf->hashcount];
	uint64_t position;

	mmh3_64_make_hashes(element, len, cbf->hashcount, hashes);

	for (int i = 0; i < cbf->hashcount; i++) {
		position = hashes[i] % cbf->size;

		if (get_counter(cbf, position) == 0) {
			return false; // element is definitely not in the filter
		}
	}

	return true;
}

/**
 * @brief Helper function for checking if a string is likely in the
 * counting Bloom filter.
 *
 * This function checks whether the specified string element is likely
 * present in the counting Bloom filter. If the function returns
 * `true`, the element is probably in the filter. If it returns
 * `false`, the element is definitely not in the filter.
 *
 * @param cbf Counting Bloom filter to use.
 * @param element Pointer to the string element to look up.
 *
 * @return `true` if the string is likely in the filter.
 * @return `false` if the string is definitely not in the filter.
 */
bool cbloom_lookup_string(const cbloomfilter *cbf, const char *element) {
	return cbloom_lookup(cbf, (uint8_t *)element, strlen(element));
}

/**
 * @brief Add an element to the counting Bloom filter.
 *
 * This function inserts the specified element into the counting Bloom
 * filter, updating the filter's counters to track the number of times
 * the element has been added.
 *
 * @param cbf Counting Bloom filter to use.
 * @param element Pointer to the element to add to the filter.
 * @param len Length of the element in bytes.
 */
void cbloom_add(cbloomfilter *cbf, void *element, const size_t len) {
	uint64_t hashes[cbf->hashcount];
	uint64_t position;

	mmh3_64_make_hashes(element, len, cbf->hashcount, hashes);

	for (int i = 0; i < cbf->hashcount; i++) {
		position = hashes[i] % cbf->size;
		inc_counter(cbf, position);
	}
}

/**
 * @brief Helper function for adding string elements to a counting
 * Bloom filter.
 *
 * This function inserts the specified string into the counting Bloom
 * filter, updating the filter's counters to track the number of times
 * the string has been added.
 *
 * @param cbf Counting Bloom filter to use.
 * @param element Pointer to the string to add to the filter.
 */
void cbloom_add_string(cbloomfilter *cbf, const char *element) {
	cbloom_add(cbf, (uint8_t *)element, strlen(element));
}

/**
 * @brief Add an element to the counting Bloom filter only if it is
 * not already present.
 *
 * This function checks for the presence of an element in the counting
 * Bloom filter.  If the element is already present (based on the
 * `cbloom_lookup` result), no action is taken.  Otherwise, the
 * element is added using `cbloom_add`.
 *
 * @param cbf Pointer to the counting Bloom filter.
 * @param element Pointer to the element data to add.
 * @param len Length of the element data.
 * @return `true` if the element was already present.
 * @return `false` if it  was newly added.
 */
bool cbloom_add_if_not_present(cbloomfilter *cbf, void *element, const size_t len) {
	if (cbloom_lookup(cbf, element, len)) {
		return true;
	}

	cbloom_add(cbf, element, len);
	return false;
}

/**
 * @brief Add a string element to the counting Bloom filter only if it
 * is not already present.
 *
 * This function is a string-specific wrapper for
 * `cbloom_add_if_not_present`. It checks for the presence of a string
 * element in the counting Bloom filter. If the element is already
 * present, no action is taken; otherwise, the element is added.
 *
 * @param cbf Pointer to the counting Bloom filter.
 * @param element Null-terminated string to add to the filter if not
 *        already present.
 * @return `true` if the element was already present.
 * @return `false if it was newly added.
 */
bool cbloom_add_if_not_present_string(cbloomfilter *cbf, const char *element) {
	return cbloom_add_if_not_present(cbf, (void *)element, strlen(element));
}

/**
 * @brief Check if an element is present in the counting Bloom filter;
 * if not, add it.
 *
 * This function performs a combined lookup and addition for the
 * element. It calculates the hash positions once, checks each
 * corresponding counter, and if any counter is zero, increments all
 * counters for the element, effectively adding it to the filter. This
 * prevents redundant hashing and improves performance.
 *
 * @param cbf Pointer to the counting Bloom filter.
 * @param element Pointer to the element data.
 * @param len Length of the element data.
 *
 * @return `true` if the element was already present.
 * @return `false` if it was newly added.
 *
 * TODO: test
 */
bool cbloom_lookup_or_add(cbloomfilter *cbf, void *element, const size_t len) {
    uint64_t hashes[cbf->hashcount];
    bool is_present = true;

    mmh3_64_make_hashes(element, len, cbf->hashcount, hashes);

    for (size_t i = 0; i < cbf->hashcount; i++) {
        uint64_t position = hashes[i] % cbf->size;
        uint64_t counter_value = get_counter(cbf, position);

        if (counter_value == 0) {
            is_present = false;
        }

		inc_counter(cbf, position);
    }

    return is_present;
}

/**
 * @brief Check if a string element is present in the counting Bloom
 * filter; if not, add it.
 *
 * This is a string-specific wrapper for `cbloom_lookup_or_add` that
 * accepts a C-style string.
 *
 * @param cbf Pointer to the counting Bloom filter.
 * @param element Null-terminated string to check and add if not present.
 *
 * @return `true` if the element was already present.
 * @return `false` if it was newly added.
 */
bool cbloom_lookup_or_add_string(cbloomfilter *cbf, const char *element) {
    return cbloom_lookup_or_add(cbf, (void *)element, strlen(element));
}

/**
 * @brief Remove an element from the counting Bloom filter.
 *
 * This function decreases the count of the specified element in the
 * counting Bloom filter. It decrements the counters associated with
 * the element, effectively "removing" the element from the filter. If
 * the element has been added multiple times, the counter will reflect
 * the remaining instances after removal.
 *
 * @param cbf Counting Bloom filter to use.
 * @param element Pointer to the element to remove from the filter.
 * @param len Length of the element in bytes.
 */
void cbloom_remove(cbloomfilter *cbf, void *element, const size_t len) {
	uint64_t hashes[cbf->hashcount];
	uint64_t positions[cbf->hashcount];

	mmh3_64_make_hashes(element, len, cbf->hashcount, hashes);

	bool shouldremove = true;
	for (size_t i = 0; i < cbf->hashcount; i++) {
		positions[i] = hashes[i] % cbf->size;
		if (get_counter(cbf, positions[i]) == 0) {
			shouldremove = false;
			break;
		}
	}

	if (shouldremove) {
		for (int i = 0; i < cbf->hashcount; i++) {
			dec_counter(cbf, positions[i]);
		}
	}
}

/**
 * @brief Clear an element from the Bloom filter if any counter
 * exceeds a specified threshold.
 *
 * This function hashes the element multiple times (based on
 * `hashcount`) and checks each associated counter to see if any
 * exceed the specified `threshold`. If any counter is above the
 * threshold, it clears all counters for the element, effectively
 * removing it from the filter.
 *
 * @param cbf Pointer to the counting Bloom filter.
 * @param element Pointer to the element data to check and clear if necessary.
 * @param len Length of the element data.
 * @param threshold The threshold above which counters will trigger clearing.
 *
 * @return `true` if the element was cleared
 * @return `false` if no counters exceeded the threshold.
 *
 * TODO: test
 */
bool cbloom_clear_if_count_above(cbloomfilter *cbf, const void *element, size_t len, size_t threshold) {
    uint64_t hashes[cbf->hashcount];
    bool     should_clear = false;

    mmh3_64_make_hashes(element, len, cbf->hashcount, hashes);

    for (size_t i = 0; i < cbf->hashcount; i++) {
        uint64_t position = hashes[i] % cbf->size;
        uint64_t counter_value = get_counter(cbf, position);

        if (counter_value > threshold) {
            should_clear = true;
            break;
        }
    }

    if (should_clear) {
        for (size_t i = 0; i < cbf->hashcount; i++) {
            uint64_t position = hashes[i] % cbf->size;
            set_counter(cbf, position, 0);
        }
    }

    return should_clear;
}

/**
 * @brief Clear a string element from the Bloom filter if any counter
 * exceeds a specified threshold.
 *
 * This function is a string-specific wrapper for
 * `cbloom_clear_if_count_above`. It hashes the provided string
 * element and checks if any associated counters exceed
 * `threshold`. If so, it clears all counters for the string element.
 *
 * @param cbf Pointer to the counting Bloom filter.
 * @param element Null-terminated string element to check and clear if
 *        necessary.
 * @param threshold The threshold above which counters will trigger clearing.
 *
 * @return `true` if the element was cleared
 * @return `false` if no counters exceeded the threshold.
 */
bool cbloom_clear_if_count_above_string(cbloomfilter *cbf, const char *element, size_t threshold) {
    return cbloom_clear_if_count_above(cbf,
									   (void *)element,
									   strlen(element),
									   threshold);
}

/**
 * @brief Apply linear decay to all counters in the counting Bloom filter.
 *
 * This function decreases each counter in the counting Bloom filter by a
 * specified `decay_amount`. If a counter's current value is less than or equal
 * to `decay_amount`, it is set to zero to avoid underflow. Zeroed counters are
 * skipped in the decay process.
 *
 * @param cbf Pointer to the counting Bloom filter.
 * @param decay_amount The amount to decrease each counter by.
 *
 * @note This function performs a fixed linear decay across all counters,
 *       reducing the impact of elements with low counts.
 *
 * TODO: test
 */
void cbloom_apply_linear_decay(cbloomfilter *cbf, uint64_t decay_amount) {
	for (size_t i = 0; i < cbf->size; i++) {
		uint64_t counter_value = get_counter(cbf, i);
		if (counter_value == 0) {
			continue; // skip zeroed counters
		}

		if (counter_value <= decay_amount) {
			counter_value = 0; // avoid underflows
		} else {
			counter_value -= decay_amount;
		}

		set_counter(cbf, i, counter_value);
	}
}

/**
 * @brief Apply exponential decay to all counters in the counting Bloom filter.
 *
 * This function applies an exponential decay factor to each counter
 * in the counting Bloom filter, effectively reducing each counter by
 * a percentage specified by `decay_factor`. Larger counter values
 * decay more slowly relative to smaller values, helping retain
 * elements with higher frequencies.
 *
 * @param cbf Pointer to the counting Bloom filter.
 * @param decay_factor The decay multiplier applied to each
 *        counter. Must be between 0.0 (full decay) and 1.0 (no
 *        decay). Values outside this range are ignored, and no decay
 *        is applied.
 *
 * @note This function does not modify zeroed counters.
 * @note If `decay_factor` is outside the range [0.0, 1.0], the
 *       function will return without applying decay. Consider adding
 *       error handling or logging for invalid `decay_factor` values.
 *
 * TODO: test
 */
void cbloom_apply_exponential_decay(cbloomfilter *cbf, float decay_factor) {
	if (decay_factor < 0.0 || decay_factor > 1.0) {
        return; // TODO error reporting?
    }

	for (size_t i = 0; i < cbf->size; i++) {
		uint64_t counter_value = get_counter(cbf, i);
		if (counter_value == 0) {
			continue; // skip zeroed counters
		}

		uint64_t decayed_value = (uint64_t)(counter_value * decay_factor);

		set_counter(cbf, i, decayed_value);
	}
}

/**
 * @brief Helper function for removing string elements from a counting
 * Bloom filter.
 *
 * This function decreases the count of the specified string in the
 * counting Bloom filter. It decrements the counters associated with
 * the string, effectively "removing" the string from the filter. If
 * the string has been added multiple times, the counter will reflect
 * the remaining instances after removal.
 *
 * @param cbf Counting Bloom filter to use.
 * @param element Pointer to the string to remove from the filter.
 */
void cbloom_remove_string(cbloomfilter *cbf, const char *element) {
	cbloom_remove(cbf, (uint8_t *)element, strlen(element));
}

/**
 * @brief Count the number of set items in a counting Bloom filter.
 *
 * This function counts how many items in the filter have been
 * set. This is useful for determining how full the filter is.
 *
 * @param cbf Counting Bloom filter to count.
 *
 * TODO: test
 */
size_t cbloom_saturation_count(const cbloomfilter *cbf) {
	size_t count = 0;

	for (size_t i = 0; i < cbf->size; i++) {
		if (get_counter(cbf, i) != 0) {
			count++;
		}
	}

	return count;
}

/**
 * @brief Calculate the saturation of a counting Bloom filter (the
 * percentage of counters set).
 *
 * This function computes the percentage of counters set to values
 * greater than 1 in the provided counting Bloom filter. This value
 * indicates how full the filter is as a percentage.
 *
 * @param cbf Counting Bloom filter to calculate saturation of.
 *
 * @return The percentage of counters set as a floating-point value.
 *
 * TODO: test
 */
float cbloom_saturation(const cbloomfilter *cbf) {
	return (float)cbloom_saturation_count(cbf) / cbf->size * 100.0;
}

/**
 * @brief Clear the contents of a counting Bloom filter.
 *
 * This function empties the counting Bloom filter, resetting all
 * counters to zero.
 *
 * @param cbf Pointer to counting Bloom filter to clear.
 *
 * TODO: test
 */
void cbloom_clear(cbloomfilter *cbf) {
	memset(cbf->countermap, 0, cbf->countermap_size);
}

/**
 * @brief Clear all counters associated with an element from the
 * counting Bloom filter.
 *
 * This function sets all positions related to a given element to
 * zero, effectively clearing the counter.
 *
 * @param cbf Pointer to the counting Bloom filter.
 * @param element Pointer to the element data to be removed.
 * @param len Length of the element data.
 * @return `true` if counters were successfully cleared, `false` otherwise.
 *
 * TODO: test
 */
bool cbloom_clear_element(cbloomfilter *cbf, void *element, size_t len) {
	uint64_t hashes[cbf->hashcount];
	uint64_t position;

	mmh3_64_make_hashes(element, len, cbf->hashcount, hashes);

	for (int i = 0; i < cbf->hashcount; i++) {
		position = hashes[i] % cbf->size;

		set_counter(cbf, position, 0);
	}

	return true;
}

/**
 * @brief Clear all counters associated with a string element from the
 * counting Bloom filter.
 *
 * This is a convenience function to handle strings with
 * `cbloom_clear_element`.
 *
 * @param cbf Pointer to the counting Bloom filter.
 * @param element Null-terminated string to clear from the Bloom filter.
 * @return `true` if counters were successfully cleared, `false` if
 *         there was an error.
 */
bool cbloom_clear_element_string(cbloomfilter *cbf, const char *element) {
    return cbloom_clear_element(cbf, (void *)element, strlen(element));
}

/**
 * @brief Save a counting Bloom filter to disk.
 *
 * This function saves the current state of the counting Bloom filter
 * to a file on disk. The file contains the filter structure followed
 * by the filter's data (e.g., counters).
 *
 * The file format is as follows:
 * - First, the `cbloomfilter` struct is saved.
 * - Then, the filter's data (e.g., counters) is saved.
 *
 * @param cbf Counting Bloom filter to save.
 * @param path File path where the filter will be saved.
 *
 * @return CBF_SUCCESS on success.
 * @return CBF_FOPEN if the file could not be opened for writing.
 * @return CBF_FWRITE if there was an error writing to the file.
 */
cbloom_error_t cbloom_save(cbloomfilter *cbf, const char *path) {
	FILE              *fp;
	cbloomfilter_file  cbff = {0};

	cbff.magic[0] = '!';
	cbff.magic[1] = 'c';
	cbff.magic[2] = 'b';
	cbff.magic[3] = 'l';
	cbff.magic[4] = 'o';
	cbff.magic[5] = 'o';
	cbff.magic[6] = 'm';
	cbff.magic[7] = '!';

	cbff.size            = cbf->size;
	cbff.csize           = cbf->csize;
	cbff.hashcount       = cbf->hashcount;
	cbff.expected        = cbf->expected;
	cbff.accuracy        = cbf->accuracy;
	cbff.countermap_size = cbf->countermap_size;
	strncpy((char *)cbff.name, cbf->name, CBLOOM_MAX_NAME_LENGTH);
	cbff.name[CBLOOM_MAX_NAME_LENGTH] = '\0';

	fp = fopen(path, "wb");
	if (fp == NULL) {
		return CBF_FOPEN;
	}

	if (fwrite(&cbff, sizeof(cbloomfilter_file), 1, fp) != 1 ||
		fwrite(cbf->countermap, cbf->countermap_size, 1, fp) != 1) {
		fclose(fp);
		return CBF_FWRITE;
	}

	fclose(fp);
	return CBF_SUCCESS;
}

/**
 * @brief Save a counting Bloom filter to a file descriptor.
 *
 * This function saves the current state of the counting Bloom filter
 * to an open file descriptor. The file format is the same as in
 * `cbloom_save`.
 *
 * @param cbf Counting Bloom filter to save.
 * @param fd File descriptor where the filter will be saved.
 *
 * @return CBF_SUCCESS on success.
 * @return CBF_FWRITE if there was an error writing to the file descriptor.
 *
 * TODO: test
 */
cbloom_error_t cbloom_save_fd(cbloomfilter *cbf, int fd) {
    cbloomfilter_file cbff = {0};

    cbff.magic[0] = '!';
    cbff.magic[1] = 'c';
    cbff.magic[2] = 'b';
    cbff.magic[3] = 'l';
    cbff.magic[4] = 'o';
    cbff.magic[5] = 'o';
    cbff.magic[6] = 'm';
    cbff.magic[7] = '!';

    cbff.size            = cbf->size;
    cbff.csize           = cbf->csize;
    cbff.hashcount       = cbf->hashcount;
    cbff.expected        = cbf->expected;
    cbff.accuracy        = cbf->accuracy;
    cbff.countermap_size = cbf->countermap_size;
    strncpy((char *)cbff.name, cbf->name, CBLOOM_MAX_NAME_LENGTH);
    cbff.name[CBLOOM_MAX_NAME_LENGTH] = '\0';

    if (write(fd, &cbff, sizeof(cbloomfilter_file)) != sizeof(cbloomfilter_file)) {
        return CBF_FWRITE;
    }

    if (write(fd, cbf->countermap, cbf->countermap_size) != (ssize_t)cbf->countermap_size) {
        return CBF_FWRITE;
    }

    return CBF_SUCCESS;
}

/**
 * @brief Load a counting Bloom filter from a file on disk.
 *
 * This function reads the state of a counting Bloom filter from a
 * specified file on disk and populates the `cbloomfilter` structure
 * with the filter's data.
 *
 * @param cbf Pointer to the counting Bloom filter struct to populate.
 * @param path File path to the counting Bloom filter file.
 *
 * @return CBF_SUCCESS on success.
 * @return CBF_FOPEN if the file could not be opened.
 * @return CBF_FREAD if there was an error reading the file.
 * @return CBF_FSTAT if the stat() system call failed on the file descriptor.
 * @return CBF_INVALIDFILE if the file format is invalid or unparseable.
 * @return CBF_OUTOFMEMORY if memory allocation failed.
 */
cbloom_error_t cbloom_load(cbloomfilter *cbf, const char *path) {
	FILE              *fp;
    struct stat        sb;
	cbloomfilter_file  cbff;

	fp = fopen(path, "rb");
	if (fp == NULL) {
		return CBF_FOPEN;
	}

	if (fstat(fileno(fp), &sb) == -1) {
		fclose(fp);
		return CBF_FSTAT;
	}

	if (fread(&cbff, sizeof(cbloomfilter_file), 1, fp) != 1) {
		fclose(fp);
		return CBF_FREAD;
	}

	cbf->size            = cbff.size;
	cbf->csize           = cbff.csize;
	cbf->hashcount       = cbff.hashcount;
	cbf->expected        = cbff.expected;
	cbf->accuracy        = cbff.accuracy;
	cbf->countermap_size = cbff.countermap_size;
	strncpy(cbf->name, (char *)cbff.name, CBLOOM_MAX_NAME_LENGTH);
	cbf->name[CBLOOM_MAX_NAME_LENGTH] = '\0';

	// basic sanity check. should fail if the file isn't valid
	if (sizeof(cbloomfilter_file) + cbf->countermap_size != sb.st_size) {
		fclose(fp);
		return CBF_INVALIDFILE;
	}

	cbf->countermap = malloc(cbf->countermap_size);
	if (cbf->countermap == NULL) {
		fclose(fp);
		return CBF_OUTOFMEMORY;
	}

	if (fread(cbf->countermap, cbf->countermap_size, 1, fp) != 1) {
		fclose(fp);
		free(cbf->countermap);
		return CBF_FREAD;
	}

	fclose(fp);

	return CBF_SUCCESS;
}

/**
 * @brief Load a counting Bloom filter from a file descriptor.
 *
 * This function reads the state of a counting Bloom filter from an
 * open file descriptor and populates the `cbloomfilter` structure.
 *
 * @param cbf Pointer to the counting Bloom filter struct to populate.
 * @param fd File descriptor from which the filter will be loaded.
 *
 * @return CBF_SUCCESS on success.
 * @return CBF_FSTAT if the stat() system call failed on the file descriptor.
 * @return CBF_FREAD if there was an error reading from the file descriptor.
 * @return CBF_INVALIDFILE if the file format is invalid or unparseable.
 * @return CBF_OUTOFMEMORY if memory allocation failed.
 *
 * TODO: test
 */
cbloom_error_t cbloom_load_fd(cbloomfilter *cbf, int fd) {
    struct stat sb;
    cbloomfilter_file cbff;

    if (fstat(fd, &sb) == -1) {
        return CBF_FSTAT;
    }

    if (read(fd, &cbff, sizeof(cbloomfilter_file)) != sizeof(cbloomfilter_file)) {
        return CBF_FREAD;
    }

    cbf->size            = cbff.size;
    cbf->csize           = cbff.csize;
    cbf->hashcount       = cbff.hashcount;
    cbf->expected        = cbff.expected;
    cbf->accuracy        = cbff.accuracy;
    cbf->countermap_size = cbff.countermap_size;
    strncpy(cbf->name, (char *)cbff.name, CBLOOM_MAX_NAME_LENGTH);
    cbf->name[CBLOOM_MAX_NAME_LENGTH] = '\0';

    if (sizeof(cbloomfilter_file) + cbf->countermap_size != sb.st_size) {
        return CBF_INVALIDFILE;
    }

    cbf->countermap = malloc(cbf->countermap_size);
    if (cbf->countermap == NULL) {
        return CBF_OUTOFMEMORY;
    }

    if (read(fd, cbf->countermap, cbf->countermap_size) != (ssize_t)cbf->countermap_size) {
        free(cbf->countermap);
        return CBF_FREAD;
    }

    return CBF_SUCCESS;
}

/**
 * @brief Return a string containing the error message corresponding
 * to an error code.
 *
 * This function converts an error code returned by a counting Bloom
 * filter function into a human-readable error message.
 *
 * @param error The error code returned by a counting Bloom filter
 * function.
 *
 * @return A pointer to a string containing the relevant error
 * message, or "Unknown error" if the error code is out of range.
 *
 * TODO test
 */
const char *cbloom_strerror(cbloom_error_t error) {
	if (error < 0 || error >= CBF_ERRORCOUNT) {
		return "Unknown error";
	}

	return cbloom_errors[error];
}
