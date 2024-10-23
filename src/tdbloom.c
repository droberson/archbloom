/**
 * @file tdbloom.c
 * @brief Implementation of a time-decaying Bloom filter.
 *
 * This file contains the implementation of a time-decaying Bloom
 * filter, which allows for approximate membership checks where
 * elements naturally expire after a set timeout period It includes
 * functions for initializing the filter, adding elements, checking
 * membership, removing expired elements, and saving/loading the
 * filter to/from disk.
 */
#include <time.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <limits.h>
#include <sys/stat.h>

#include "tdbloom.h"
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
 * @param accuracy The desired rate of false positives (eg 0,01 for 99.99% accuracy).
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
 */
static time_t get_monotonic_time() {
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);

	return ts.tv_sec;
}

/**
 * @brief Initializes a time-decaying Bloom filter.
 *
 * This function sets up a time-decaying Bloom filter based on the
 * expected number of elements, desired accuracy, and the specified
 * timeout (the duration for which elements remain valid).
 *
 * @param tdbf Pointer to a time-decaying Bloom filter structure to initialize.
 * @param expected Maximum expected number of elements to store in the filter.
 * @param accuracy Acceptable false positive rate (e.g., 0.01 for 99.99% accuracy).
 * @param timeout Number of seconds an element remains valid before expiring.
 *
 * @return TDBF_SUCCESS on success.
 * @return TDBF_INVALIDTIMEOUT if the value of `timeout` is invalid.
 * @return TDBF_OUTOFMEMORY if memory allocation fails.
 */
tdbloom_error_t tdbloom_init(tdbloom *tdbf, const size_t expected, const float accuracy, const size_t timeout) {
	tdbf->size       = ideal_size(expected, accuracy);
	tdbf->hashcount  = (tdbf->size / expected) * log(2);
	tdbf->timeout    = timeout;
	tdbf->expected   = expected;
	tdbf->accuracy   = accuracy;
	tdbf->start_time = get_monotonic_time();

	// determine which datatype to use for storing timestamps
	/// TODO: test this
	int bytes;
	if (timeout > UINT64_MAX || sizeof(time_t) == 4 && timeout > UINT32_MAX) {
		return TDBF_INVALIDTIMEOUT;
	}

	if      (timeout < UINT8_MAX)   { bytes = 1; tdbf->max_time = UINT8_MAX; }
	else if (timeout < UINT16_MAX)  { bytes = 2; tdbf->max_time = UINT16_MAX; }
	else if (timeout < UINT32_MAX)  { bytes = 4; tdbf->max_time = UINT32_MAX; }
	else if (timeout <= UINT64_MAX) { bytes = 8; tdbf->max_time = UINT64_MAX; }

	tdbf->bytes = bytes;

	tdbf->filter = calloc(tdbf->size, bytes);
	if (tdbf->filter == NULL) {
		return TDBF_OUTOFMEMORY;
	}

	// calculate filter size
	tdbf->filter_size = tdbf->size * tdbf->bytes;

	return TDBF_SUCCESS;
}

/**
 * @brief Destroys a time-decaying Bloom filter and free associated resources.
 *
 * This function uninitializes a time-decaying Bloom filter, releasing
 * any memory that was allocated for it.
 *
 * @param tdbf Pointer to a time-decaying Bloom filter to destroy.
 */
void tdbloom_destroy(tdbloom *tdbf) {
	if (tdbf->filter) {
		free(tdbf->filter);
		tdbf->filter = NULL;
	}
}

/**
 * @brief Clear the contents of a time-decaying Bloom filter and
 * reset the start time.
 *
 * This function clears all elements in the time-decaying Bloom filter
 * and resets the start time to the current time (`now`), effectively
 * resetting the filter.
 *
 * @param tdbf Pointer to the time-decaying Bloom filter to clear.
 *
 * TODO: test
 */
void tdbloom_clear(tdbloom *tdbf) {
	memset(tdbf->filter, 0, tdbf->filter_size);
	tdbf->start_time = get_monotonic_time();
}

/**
 * @brief Reset the start time of a time-decaying Bloom filter to the
 * current time.
 *
 * This function resets the start time of the time-decaying Bloom
 * filter to the current time (`now`). This could be useful in
 * scenarios where you want to pause the filter while preserving its
 * data.
 *
 * @param tdbf Pointer to the time-decaying Bloom filter for which the
 * start time will be reset.
 *
 * TODO: test
 */
void tdbloom_reset_start_time(tdbloom *tdbf) {
	tdbf->start_time = get_monotonic_time();
}

/**
 * @brief Removes expired data from a time-decaying Bloom filter.
 *
 * This function clears expired elements from the time-decaying Bloom
 * filter. It removes any elements that have exceeded their valid
 * time window.
 *
 * @param tdbf Pointer to the time-decaying Bloom filter to clear
 * expired data from.
 *
 * @return The number of expired items removed from the filter.
 *
 * TODO: test
 */
size_t tdbloom_clear_expired(tdbloom *tdbf) {
	time_t now    = get_monotonic_time();
	size_t ts     = ((now - tdbf->start_time + tdbf->max_time) % tdbf->max_time) + 1;
	size_t reaped = 0;

	for (size_t i = 0; i < tdbf->size; i++) {
		uint64_t value;
		switch (tdbf->bytes) {
		case 1: value = ((uint8_t *)tdbf->filter)[i];  break;
		case 2: value = ((uint16_t *)tdbf->filter)[i]; break;
		case 4: value = ((uint32_t *)tdbf->filter)[i]; break;
		case 8: value = ((uint64_t *)tdbf->filter)[i]; break;
		}

		// set to 0 if expired
		if (value != 0 && ((ts - value + tdbf->max_time) % tdbf->max_time) > tdbf->timeout) {
			switch (tdbf->bytes) {
			case 1: ((uint8_t *)tdbf->filter)[i] = 0;  break;
			case 2: ((uint16_t *)tdbf->filter)[i] = 0; break;
			case 4: ((uint32_t *)tdbf->filter)[i] = 0; break;
			case 8: ((uint64_t *)tdbf->filter)[i] = 0; break;
			}

			reaped++;
		}
	}

	return reaped;
}

/**
 * @brief Counts the number of expired items in a time-decaying Bloom filter.
 *
 * This function returns the number of items in the time-decaying
 * Bloom filter that have expired and are no longer valid.
 *
 * @param tdbf Time-decaying Bloom filter to count expired items in.
 *
 * @return The number of expired items in the filter.
 */
size_t tdbloom_count_expired(const tdbloom tdbf) {
	time_t now = get_monotonic_time();
	size_t ts = ((now - tdbf.start_time + tdbf.max_time) % tdbf.max_time) + 1;
	size_t expired = 0;

	for (size_t i = 0; i < tdbf.size; i++) {
		uint64_t value;
		switch (tdbf.bytes) {
		case 1: value = ((uint8_t *)tdbf.filter)[i];  break;
		case 2: value = ((uint16_t *)tdbf.filter)[i]; break;
		case 4: value = ((uint32_t *)tdbf.filter)[i]; break;
		case 8: value = ((uint64_t *)tdbf.filter)[i]; break;
		}

		// set to 0 if expired
		if (value != 0 && ((ts - value + tdbf.max_time) % tdbf.max_time) > tdbf.timeout) {
			expired++;
		}
	}

	return expired;
}

/**
 * @brief Calculates the saturation of a time-decaying Bloom filter.
 *
 * This function computes the saturation of the time-decaying Bloom
 * filter, expressed as the percentage of bits that are set,
 * indicating how full the filter is.
 *
 * @param tdbf Time-decaying Bloom filter to calculate saturation of.
 *
 * @return The percentage of saturation in the filter.
 *
 * TODO: test this
 */
float tdbloom_saturation(const tdbloom tdbf) {
	size_t irrelevant = 0;
	time_t now = get_monotonic_time();
	size_t ts = ((now - tdbf.start_time + tdbf.max_time) % tdbf.max_time) + 1;

	for (size_t i = 0; i < tdbf.size; i++) {
		size_t value;
		switch(tdbf.bytes) {
		case 1: value = ((uint8_t *)tdbf.filter)[i]; break;
		case 2: value = ((uint16_t *)tdbf.filter)[i]; break;
		case 4: value = ((uint32_t *)tdbf.filter)[i]; break;
		case 8: value = ((uint64_t *)tdbf.filter)[i]; break;
		}

		if (value == 0 || ((ts - value + tdbf.max_time) % tdbf.max_time) > tdbf.timeout) {
			irrelevant++;
		}
	}

	float saturation = 1.0 - ((float)irrelevant / tdbf.size);
	return saturation * 100;
}

/**
 * @brief Add an element to a time-decaying Bloom filter.
 *
 * This function inserts an element into the time-decaying Bloom
 * filter, using the current time to track its validity based on the
 * filter's timeout settings.
 *
 * @param tf Pointer to the time-decaying Bloom filter to add the element to.
 * @param element Pointer to the element to add to the filter.
 * @param len Length of the element in bytes.
 */
void tdbloom_add(tdbloom *tf, const void *element, const size_t len) {
	uint64_t    result;
	uint64_t    hashes[tf->hashcount];
	time_t      now = get_monotonic_time();
	size_t      ts = ((now - tf->start_time) % tf->max_time + tf->max_time) % tf->max_time + 1;

	mmh3_64_make_hashes(element, len, tf->hashcount, hashes);

	for (int i = 0; i < tf->hashcount; i++) {
		result = hashes[i] % tf->size;
		switch(tf->bytes) {
		case 1:	((uint8_t *)tf->filter)[result]  = ts; break;
		case 2:	((uint16_t *)tf->filter)[result] = ts; break;
		case 4:	((uint32_t *)tf->filter)[result] = ts; break;
		case 8: ((uint64_t *)tf->filter)[result] = ts; break;
		}
	}
}

/**
 * @brief Add a string element to a time-decaying Bloom filter.
 *
 * This function inserts a string element into the time-decaying Bloom
 * filter, using the current time to track its validity based on the
 * filter's timeout settings.
 *
 * @param tdbf Time-decaying Bloom filter to add the string element to.
 * @param element Pointer to the string element to add to the filter.
 */
void tdbloom_add_string(tdbloom tdbf, const char *element) {
	tdbloom_add(&tdbf, (uint8_t *)element, strlen(element));
}

/**
 * @brief Check if an element exists in a time-decaying Bloom filter.
 *
 * This function checks whether the given element exists in the
 * time-decaying Bloom filter and whether it is still valid (i.e., has
 * not expired based on the timeout settings).
 *
 * @param tdbf Time-decaying Bloom filter to perform the lookup against.
 * @param element Pointer to the element to search for in the filter.
 * @param len Length of the element in bytes.
 *
 * @return true if the element is likely in the filter and valid
 * @return false if it is definitely not in the filter or has expired.
 */
bool tdbloom_lookup(const tdbloom tdbf, const void *element, const size_t len) {
	uint64_t    result;
	uint64_t    hashes[tdbf.hashcount];
	time_t      now = get_monotonic_time();
	size_t      ts = ((now - tdbf.start_time) % tdbf.max_time + tdbf.max_time) % tdbf.max_time + 1;

	if ((now - tdbf.start_time) > tdbf.max_time) { return false; }

	mmh3_64_make_hashes(element, len, tdbf.hashcount, hashes);

	for (int i = 0; i < tdbf.hashcount; i++) {
		result = hashes[i] % tdbf.size;

		size_t value;
		switch(tdbf.bytes) {
		case 1:	value = ((uint8_t *)tdbf.filter)[result];  break;
		case 2:	value = ((uint16_t *)tdbf.filter)[result]; break;
		case 4:	value = ((uint32_t *)tdbf.filter)[result]; break;
		case 8:	value = ((uint64_t *)tdbf.filter)[result]; break;
		}

		if (value == 0 ||
			((ts - value + tdbf.max_time) % tdbf.max_time) > tdbf.timeout) {
			return false;
		}
	}

	return true;
}

/**
 * @brief Check if a string element exists in a time-decaying Bloom filter.
 *
 * This function checks whether the given string element exists in the
 * time-decaying Bloom filter and whether it is still valid (i.e., has
 * not expired based on the timeout settings).
 *
 * @param tdbf Time-decaying Bloom filter to perform the lookup against.
 * @param element Pointer to the string element to search for.
 *
 * @return true if the element is likely in the filter and valid
 * @return false if it is definitely not in the filter or has expired.
 */
bool tdbloom_lookup_string(const tdbloom tdbf, const char *element) {
	return tdbloom_lookup(tdbf, (uint8_t *)element, strlen(element));
}

/**
 * @brief Check if an element has expired in a time-decaying Bloom filter.
 *
 * This function checks whether the given element has expired in the
 * time-decaying Bloom filter, meaning it was once present but its
 * validity has elapsed.
 *
 * @param tdbf Time-decaying Bloom filter to check.
 * @param element Pointer to the element to check for expiration.
 * @param len Length of the element in bytes.
 *
 * @return true if the element has expired, false if the element is either still valid or never existed.
 *
 * TODO: test
 */
bool tdbloom_has_expired(const tdbloom tdbf, const void *element, size_t len) {
	uint64_t result;
	uint64_t hashes[tdbf.hashcount];
	time_t   now = get_monotonic_time();
	size_t   ts  = ((now - tdbf.start_time) % tdbf.max_time + tdbf.max_time) % tdbf.max_time + 1;

	mmh3_64_make_hashes(element, len, tdbf.hashcount, hashes);

	for (size_t i = 0; i < tdbf.hashcount; i++) {
		result = hashes[i] % tdbf.size;

		size_t value;
		switch (tdbf.bytes) {
		case 1: value = ((uint8_t *)tdbf.filter)[result];  break;
		case 2: value = ((uint16_t *)tdbf.filter)[result]; break;
		case 4: value = ((uint32_t *)tdbf.filter)[result]; break;
		case 8: value = ((uint64_t *)tdbf.filter)[result]; break;
		}

		if (value != 0 && ((ts - value + tdbf.max_time) % tdbf.max_time) > tdbf.timeout) {
			return true; // element has expired
		}
	}

	return false; // element has not expired
}

/**
 * @brief Check if a string element has expired in a time-decaying Bloom filter.
 *
 * This function checks whether a given string element has expired in
 * the time-decaying Bloom filter, meaning it was once present but its
 * validity has elapsed.
 *
 * @param tdbf Time-decaying Bloom filter to check.
 * @param element Pointer to the string element to check for expiration.
 *
 * @return true if the element has expired.
 * @return false if the element is either still valid or never existed.
 *
 * TODO: test
 */
bool tdbloom_has_expired_string(const tdbloom tdbf, const char *element) {
	return tdbloom_has_expired(tdbf, element, strlen(element));
}

/**
 * @brief Check if an element has expired in a time-decaying Bloom
 * filter and reset it if has expired.
 *
 * This function checks whether the given element has expired in the
 * time-decaying Bloom filter. If the element has expired, it is reset
 * by updating its timestamp and counter to make it valid again.
 *
 * @param tdbf Pointer to the time-decaying Bloom filter.
 * @param element Pointer to the element to check and possibly reset.
 * @param len Length of the element in bytes.
 *
 * @return true if the element was expired and reset.
 * @return false if the element was still valid or did not exist.
 *
 * TODO: test
 */
bool tdbloom_reset_if_expired(tdbloom *tdbf, const void *element, size_t len) {
	if (tdbloom_has_expired(*tdbf, element, len)) {
		tdbloom_add(tdbf, element, len);
		return true; // element was expired and has been reset
	}

	return false; // element still valid or never existed.
}

/**
 * @brief Check if a string element has expired in a time-decaying
 * Bloom filter and reset it if has expired.
 *
 * This function checks whether a given string element has expired in
 * the time-decaying Bloom filter. If the element has expired, it is
 * reset by updating its timestamp and counter to make it valid again.
 *
 * @param tdbf Pointer to the time-decaying Bloom filter.
 * @param element Pointer to the string element to check and possibly reset.
 *
 * @return true if the element was expired and reset.
 * @return false if the element was still valid or did not exist.
 *
 * TODO: test
 */
bool tdbloom_reset_if_expired_string(tdbloom *tdbf, const char *element) {
	return tdbloom_reset_if_expired(tdbf, element, strlen(element));
}

/**
 * @brief Save a time-decaying Bloom filter to disk.
 *
 * This function saves the current state of a time-decaying Bloom
 * filter to a file on disk.  The file contains the structure of the
 * Bloom filter followed by the filter's bitmap.
 *
 * The file format is as follows:
 * - First, the `tdbloom` struct is saved.
 * - Then, the bitmap of the Bloom filter is saved.
 *
 * @param tdbf Time-decaying Bloom filter to save.
 * @param path File path where the Bloom filter will be saved.
 *
 * @return TDBF_SUCCESS on success.
 * @return TDBF_FOPEN if the file could not be opened for writing.
 * @return TDBF_FWRITE if there was an error writing to the file.
 *
 * TODO: test
 */
tdbloom_error_t tdbloom_save(tdbloom tdbf, const char *path) {
	FILE *fp;

	fp = fopen(path, "wb");
	if (fp == NULL) {
		return TDBF_FOPEN;
	}

	if (fwrite(&tdbf, sizeof(tdbloom), 1, fp) != 1 ||
		fwrite(tdbf.filter, tdbf.filter_size, 1, fp)) {
		fclose(fp);
		return TDBF_FWRITE;
	}

	fclose(fp);

	return TDBF_SUCCESS;
}

/**
 * @brief Load a time-decaying Bloom filter from a file on disk.
 *
 * This function reads a time-decaying Bloom filter from the specified
 * file and initializes the filter structure with the saved data.
 *
 * @param tdbf Pointer to the `tdbloom` struct to initialize.
 * @param path File path from which to load the Bloom filter.
 *
 * @return TDBF_SUCCESS on success.
 * @return TDBF_FOPEN if the file could not be opened.
 * @return TDBF_FREAD if there was an error reading from the file.
 * @return TDBF_FSTAT if the fstat() system call fails.
 * @return TDBF_INVALIDFILE if the file format is incorrect.
 * @return TDBF_OUTOFMEMORY if memory allocation fails.
 *
 * TODO: test
 */
tdbloom_error_t tdbloom_load(tdbloom *tdbf, const char *path) {
	FILE        *fp;
	struct stat  sb;

	fp = fopen(path, "rb");
	if (fp == NULL) {
		return TDBF_FOPEN;
	}

	if (fstat(fileno(fp), &sb) == -1) {
		fclose(fp);
		return TDBF_FSTAT;
	}

	if (fread(tdbf, sizeof(tdbloom), 1, fp) != 1) {
		fclose(fp);
		return TDBF_FREAD;
	}

	// basic sanity checks. should fail if file is not a filter
	if (tdbf->filter_size != (tdbf->size * tdbf->bytes) ||
		(sizeof(tdbloom) + tdbf->filter_size) != sb.st_size) {
		fclose(fp);
		return TDBF_INVALIDFILE;
	}

	tdbf->filter = malloc(tdbf->filter_size);
	if (tdbf->filter == NULL) {
		fclose(fp);
		return TDBF_OUTOFMEMORY;
	}

	if (fread(tdbf->filter, tdbf->filter_size, 1, fp) != 1) {
		free(tdbf->filter);
		tdbf->filter = NULL;
		fclose(fp);
		return TDBF_FREAD;
	}

	fclose(fp);

	return TDBF_SUCCESS;
}

/**
 * @brief Return a string containing the error message for a given error code.
 *
 * This function converts an error code returned by a time-decaying
 * Bloom filter function into a human-readable error message.
 *
 * @param error The error code returned by a time-decaying Bloom
 * filter function.
 *
 * @return A pointer to a string containing the relevant error
 * message, or "Unknown error" if the error code is out of range.
 *
 * TODO: test
 */
const char *tdbloom_strerror(tdbloom_error_t error) {
	if (error < 0 || error >= TDBF_ERRORCOUNT) {
		return "Unknown error";
	}

	return tdbloom_errors[error];
}
