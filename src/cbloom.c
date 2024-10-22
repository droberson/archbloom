/* cbloom.c
 */
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <sys/stat.h>

#include "mmh3.h"
#include "cbloom.h"


/* ideal_size() -- calculate ideal size of a filter
 *
 * Args:
 *     expected - maximum expected number of elements
 *     accuracy - margin of error. ex: use 0.01 if you want 99.99% accuracy
 *
 * Returns:
 *     unsigned integer
 */
static uint64_t ideal_size(const uint64_t expected, const float accuracy) {
	return -(expected * log(accuracy) / pow(log(2.0), 2));
}

/* cbloom_init() -- initialize counting bloom filter
 *
 * Args:
 *     cbf      - filter to initialize
 *     expected - expected number of elements in filter
 *     accuracy - margin of acceptable error. ex: 0.01 is "99.99%" accurate
 *     csize    - size of counter: COUNTER_8BIT, _16BIT, _32BIT, _64BIT, ...
 *
 * Returns:
 *     CBF_SUCCESS (0) on success
 *     corresponding error value on failure
 */
cbloom_error_t cbloom_init(cbloomfilter *cbf, const size_t expected, const float accuracy, counter_size csize) {
	cbf->size      = ideal_size(expected, accuracy);
	// add 0.5 to round up/down
	cbf->hashcount = (uint64_t)((cbf->size / expected) * log(2) + 0.5);
	cbf->csize     = csize;

	switch (csize) {
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

/* cbloom_destroy() -- free memory allocated by cbloom_init()
 *
 * Args:
 *     cbf - filter to free
 *
 * Returns:
 *     Nothing
 */
void cbloom_destroy(cbloomfilter *cbf) {
	if (cbf->countermap) {
		free(cbf->countermap);
		cbf->countermap = NULL;
	}
}

/* get_counter, inc_counter, dec_counter -- helper functions used to handle
 *     different sized counters.
 */
static inline uint64_t get_counter(const cbloomfilter *cbf, uint64_t position) {
	switch (cbf->csize) {
	case COUNTER_8BIT:	return  ((uint8_t *)cbf->countermap)[position];
	case COUNTER_16BIT:	return ((uint16_t *)cbf->countermap)[position];
	case COUNTER_32BIT:	return ((uint32_t *)cbf->countermap)[position];
	case COUNTER_64BIT:	return ((uint64_t *)cbf->countermap)[position];
	default:
		return 0; // shouldn't get here
	}
}

static void inc_counter(cbloomfilter *cbf, uint64_t position) {
	switch (cbf->csize) {
	case COUNTER_8BIT:
		if (((uint8_t *)cbf->countermap)[position] != UINT8_MAX) {
			((uint8_t *)cbf->countermap)[position]++;
		}
		break;
	case COUNTER_16BIT:
		if (((uint16_t *)cbf->countermap)[position] != UINT16_MAX) {
			((uint16_t *)cbf->countermap)[position]++;
		}
		break;
	case COUNTER_32BIT:
		if (((uint32_t *)cbf->countermap)[position] != UINT32_MAX) {
			((uint32_t *)cbf->countermap)[position]++;
		}
		break;
	case COUNTER_64BIT:
		if (((uint64_t *)cbf->countermap)[position] != UINT64_MAX) {
			((uint64_t *)cbf->countermap)[position]++;
		}
		break;
	default:
		return; // shouldn't get here
	}
}

static void dec_counter(cbloomfilter *cbf, uint64_t position) {
	switch (cbf->csize) {
		case COUNTER_8BIT:
		if (((uint8_t *)cbf->countermap)[position] > 0) {
			((uint8_t *)cbf->countermap)[position]--;
		}
		break;
	case COUNTER_16BIT:
		if (((uint16_t *)cbf->countermap)[position] > 0) {
			((uint16_t *)cbf->countermap)[position]--;
		}
		break;
	case COUNTER_32BIT:
		if (((uint32_t *)cbf->countermap)[position] > 0) {
			((uint32_t *)cbf->countermap)[position]--;
		}
		break;
	case COUNTER_64BIT:
		if (((uint64_t *)cbf->countermap)[position] > 0) {
			((uint64_t *)cbf->countermap)[position]--;
		}
		break;
	default:
		return; // shouldn't get here
	}
}

/* cbloom_count() -- get approximate count of an element in the filter
 *
 * Args:
 *     cbf     - filter to use
 *     element - element to count
 *     len     - length of element
 *
 * Returns:
 *     size_t representing the approximate count of 'element' in the filter
 *
 * TODO: test this
 */
size_t cbloom_count(const cbloomfilter cbf, void *element, size_t len) {
	uint64_t hash[2];
	uint64_t position;
	uint64_t count = UINT64_MAX;

	for (int i = 0; i < cbf.hashcount; i++) {
		mmh3_128(element, len, i, hash);
		position = ((hash[0] % cbf.size) + (hash[1] % cbf.size)) % cbf.size;

		uint64_t current_count = get_counter(&cbf, position);
		if (current_count < count) {
			count = current_count;
		}
	}

	return count;
}

/* cbloom_count_string() -- helper function to get approximate count of
 *                                 a string element in the filter.
 *
 * Args:
 *     cbf     - filter to use
 *     element - element to count
 *
 * Returns:
 *     size_t representing the approximate count of 'element' in the filter
 */

size_t cbloom_count_string(const cbloomfilter cbf, char *element) {
	return cbloom_count(cbf, (uint8_t *)element, strlen(element));
}

/* cbloom_lookup() -- check if an element is likely in the filter
 *
 * Args:
 *     cbf     - filter to use
 *     element - element to look up
 *     len     - length of element in bytes
 *
 * Returns:
 *     true if element is probably in the filter
 *     false if element is definitely not in the filter
 */
bool cbloom_lookup(const cbloomfilter cbf, void *element, const size_t len) {
	uint64_t hash[2];
	uint64_t position;

	for (int i = 0; i < cbf.hashcount; i++) {
		mmh3_128(element, len, i, hash);
		position = ((hash[0] % cbf.size) + (hash[1] % cbf.size)) % cbf.size;

		// if any counter is zero, element isn't in the filter
		if (get_counter(&cbf,position) == 0) {
			return false;
		}
	}

	return true;
}

/* cbloom_lookup_string() -- helper function for looking up strings
 *
 * Args:
 *     cbf     - filter to use
 *     element - string to look up
 *
 * Returns:
 *     true if element is probably in the set
 *     false if element is definitely not in the set
 */
bool cbloom_lookup_string(const cbloomfilter cbf, const char *element) {
	return cbloom_lookup(cbf, (uint8_t *)element, strlen(element));
}

/* cbloom_add() -- add an element to a counting bloom filter
 *
 * Args:
 *     cbf     - filter to use
 *     element - element to add
 *     len     - length of element in bytes
 *
 * Returns:
 *     Nothing
 */
void cbloom_add(cbloomfilter cbf, void *element, const size_t len) {
	uint64_t hash[2];
	uint64_t position;

	for (int i = 0; i < cbf.hashcount; i++) {
		mmh3_128(element, len, i, hash);
		position = ((hash[0] % cbf.size) + (hash[1] % cbf.size)) % cbf.size;
		inc_counter(&cbf, position);
	}
}

/* cbloom_add_string() -- helper function for adding strings
 *
 * Args:
 *     cbf     - filter to use
 *     element - string to add to the filter
 *
 * Returns:
 *     Nothing
 */
void cbloom_add_string(cbloomfilter cbf, const char *element) {
	cbloom_add(cbf, (uint8_t *)element, strlen(element));
}

/* cbloom_remove() -- remove an element from a counting bloom filter
 *
 * Args:
 *     cbf     - filter to use
 *     element - element to remove
 *     len     - length of element in bytes
 *
 * Returns:
 *     Nothing
 */
void cbloom_remove(cbloomfilter cbf, void *element, const size_t len) {
	uint64_t hash[2];
	uint64_t positions[cbf.hashcount];

	bool shouldremove = true;
	for (int i = 0; i < cbf.hashcount; i++) {
		mmh3_128(element, len, i, hash);
		positions[i] = ((hash[0] % cbf.size) + (hash[1] % cbf.size)) % cbf.size;
		if (get_counter(&cbf, positions[i]) == 0) {
			shouldremove = false;
			break;
		}
	}

	if (shouldremove) {
		for (int i = 0; i < cbf.hashcount; i++) {
			dec_counter(&cbf, positions[i]);
		}
	}
}

/* cbloom_remove_string() -- helper function to remove strings
 *
 * Args:
 *     cbf     - filter to use
 *     element - string to remove
 *
 * Returns:
 *     Nothing
 */
void cbloom_remove_string(cbloomfilter cbf, const char *element) {
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
size_t cbloom_saturation_count(const cbloomfilter cbf) {
	size_t count = 0;

	for (size_t i = 0; i < cbf.size; i++) {
		if (get_counter(&cbf, i) != 0) {
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
float cbloom_saturation(const cbloomfilter cbf) {
	return (float)cbloom_saturation_count(cbf) / cbf.size * 100.0;
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

/* cbloom_save() -- save a counting bloom filter to disk
 *
 * Format of these files on disk:
 *    +------------------------------+
 *    | counting bloom filter struct |
 *    +------------------------------+
 *    |             data             |
 *    +------------------------------+
 *
 * Args:
 *     cbf  - filter to save
 *     path - path to save filter
 *
 * Returns:
 *     CBF_SUCCESS on success
 *     CBF_FOPEN if unable to open file
 *     CBF_FWRITE if unable to write to file
 */
cbloom_error_t cbloom_save(cbloomfilter cbf, const char *path) {
	FILE        *fp;
	struct stat  sb;

	fp = fopen(path, "wb");
	if (fp == NULL) {
		return CBF_FOPEN;
	}

	if (fwrite(&cbf, sizeof(cbloomfilter), 1, fp) != 1 ||
		fwrite(cbf.countermap, cbf.countermap_size, 1, fp) != 1) {
		fclose(fp);
		return CBF_FWRITE;
	}

	fclose(fp);
	return CBF_SUCCESS;
}

/* cbloom_load() -- load a counting bloom filter from disk
 *
 * Args:
 *     cbf  - counting bloom filter struct to populate
 *     path - path on disk to counting bloom filter file
 *
 * Returns:
 *     CBF_SUCCESS on success
 *     CBF_FOPEN if unable to open file
 *     CBF_FREAD if unable to read file
 *     CBF_FSTAT if unable to stat() file descriptor
 *     CBF_INVALIDFILE if file is unable to be parsed
 *     CBF_OUTOFMEMORY if out of memory
 */
cbloom_error_t cbloom_load(cbloomfilter *cbf, const char *path) {
	FILE        *fp;
    struct stat  sb;

	fp = fopen(path, "rb");
	if (fp == NULL) {
		return CBF_FOPEN;
	}

	if (fstat(fileno(fp), &sb) == -1) {
		fclose(fp);
		return CBF_FSTAT;
	}

	if (fread(cbf, sizeof(cbloomfilter), 1, fp) != 1) {
		fclose(fp);
		free(cbf->countermap);
		cbf->countermap = NULL;
		return CBF_FREAD;
	}

	// basic sanity check. should fail if the file isn't valid
	if (sizeof(cbloomfilter) + cbf->countermap_size != sb.st_size) {
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


/* cbloom_strerror() -- returns string containing error message
 *
 * Args:
 *     error - error number returned from function
 *
 * Returns:
 *     "Unknown error" if 'error' is out of range. Otherwise, a pointer to
 *     a string containing relevant error message.
 *
 * TODO test
 */
const char *cbloom_strerror(cbloom_error_t error) {
	if (error < 0 || error >= CBF_ERRORCOUNT) {
		return "Unknown error";
	}

	return cbloom_errors[error];
}
