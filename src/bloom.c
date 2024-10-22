/* bloom.c
 * TODO: name filters
 */
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>

#include "mmh3.h"
#include "bloom.h"

/* ideal_size() - calculate ideal size of a filter
 *
 * Args:
 *     expected - maximum expected number of elements
 *     accuracy - margin of error. ex: use 0.01 if you want 99.99% accuracy
 *
 * Returns:
 *     unsigned integer
 */
static size_t ideal_size(const size_t expected, const float accuracy) {
	return -(expected * log(accuracy) / pow(log(2.0), 2));
}

/* bloom_init() - initialize a bloom filter
 *
 * Args:
 *     bf       - bloomfilter structure
 *     expected - expected number of elements
 *     accuracy - margin of acceptable error. ex: 0.01 is "99.99%" accurate
 *
 * Returns:
 *     BF_SUCCESS on success
 *     BF_OUTOFMEMORY if memory allocation failed
 *
 * TODO: test
 */
bloom_error_t bloom_init(bloomfilter *bf, const size_t expected, const float accuracy) {
	bf->size        = ideal_size(expected, accuracy);
	bf->hashcount   = (bf->size / expected) * log(2);
	bf->bitmap_size = bf->size / 8;
	bf->expected    = expected;
	bf->accuracy    = accuracy;
	bf->insertions  = 0;

	bf->bitmap      = calloc(bf->bitmap_size, sizeof(uint8_t));
	if (bf->bitmap == NULL) {
		return BF_OUTOFMEMORY;
	}

	return BF_SUCCESS;
}

/* bloom_destroy() - free a bloom filter's allocated memory
 *
 * Args:
 *     bf - filter to free
 *
 * Returns:
 *     Nothing
 */
void bloom_destroy(bloomfilter *bf) {
	if (bf->bitmap) {
		free(bf->bitmap);
		bf->bitmap = NULL;
	}
}

/* bloom_clear() - clear the contents of a bloom filter and reset insertion
 *                 counter to zero.
 *
 * Args:
 *     bf - filter to clear
 *
 * Returns:
 *     Nothing
 *
 * TODO: test
 */
void bloom_clear(bloomfilter *bf) {
	memset(bf->bitmap, 0, bf->bitmap_size);
	bf->insertions = 0;
}

/* bloom_saturation_count() - calculate number of bits set to 1 in bloom filter
 *
 * Args:
 *     bf - filter to count
 *
 * Returns:
 *     number of bits set to 1 in filter
 */
// lookup table for bloom_saturation() and bloom_saturation_count()
static const uint8_t bit_count_table[256] = {
    0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4,
    1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
    1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
    1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
    3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
    4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8
};

// TODO: test
size_t bloom_saturation_count(const bloomfilter bf) {
	size_t count = 0;

	for (size_t i = 0; i < bf.bitmap_size; i++) {
		count += bit_count_table[bf.bitmap[i]];
	}

	return count;
}

/* bloom_saturation() - calculate saturation (percentage of bits set in filter)
 *
 * Args:
 *     bf - filter to calculate saturation of
 *
 * Returns:
 *     percentage of bits set in filter
 *
 * TODO: test
 */
float bloom_saturation(const bloomfilter bf) {
	size_t total_bits = bf.bitmap_size * 8;
	size_t set_bits   = bloom_saturation_count(bf);

	return (float)set_bits / total_bits * 100.0;
}

/* bloom_capacity() - returns the capacity of a bloom filter as a percentage
 *                    based on number of insertions and the expected number of
 *                    elements within the filter.
 *
 * Args:
 *     bf - filter to check capacity
 *
 * Returns:
 *     a double representing the capacity of the bloom filter
 */
double bloom_capacity(const bloomfilter bf) {
	return ((double)bf.insertions / (double)bf.expected) * 100.0;
}

/* calculate_positions() - helper function to get bit positions
 *
 * Args:
 *     result -
 *     byte_position - pointer to store byte position
 *     bit_position  - pointer to store bit positoin
 *
 * Returns:
 *     Nothing
 */
static inline void calculate_positions(uint64_t result, uint64_t *byte_position, uint8_t *bit_position) {
	*byte_position = result / 8;
	*bit_position = result % 8;
}

/* bloom_lookup() - check if an element is likely in a filter
 *
 * Args:
 *     bf      - filter to use
 *     element - element to lookup
 *     len     - element length in bytes
 *
 * Returns:
 *     true if element is probably in filter
 *     false if element is definitely not in filter
 */
bool bloom_lookup(const bloomfilter bf, const void *element, const size_t len) {
	uint64_t hash[2];
	uint64_t result;
	uint64_t byte_position;
	uint8_t  bit_position;

	for (size_t i = 0; i < bf.hashcount; i++) {
		mmh3_128(element, len, i, hash);
		result = ((hash[0] % bf.size) + (hash[1] % bf.size)) % bf.size;

		calculate_positions(result, &byte_position, &bit_position);

		if ((bf.bitmap[byte_position] & (0x01 << bit_position)) == 0) {
			return false;
		}
	}

	return true;
}

/* bloom_lookup_string() - helper function for bloom_lookup() to handle strings
 *
 * Args:
 *     bf      - filter to use
 *     element - element to lookup
 *
 * Returns
 *     true if element is likely in the filter
 *     false if element is definitely not in the filter
 */
bool bloom_lookup_string(const bloomfilter bf, const char *element) {
	return bloom_lookup(bf, (uint8_t *)element, strlen(element));
}

/* bloom_add() - add/insert an element into a bloom filter
 *
 * Args:
 *     bf      - filter to use
 *     element - element to add
 *     len     - element length in bytes
 *
 * Returns:
 *     Nothing
 */
void bloom_add(bloomfilter *bf, const void *element, const size_t len) {
	uint64_t  hash[2];
	uint64_t  result;
	uint64_t  byte_position;
	uint8_t   bit_position;
	bool      all_bits_set = true;

	for (size_t i = 0; i < bf->hashcount; i++) {
		mmh3_128(element, len, i, hash);
		result = ((hash[0] % bf->size) + (hash[1] % bf->size)) % bf->size;

		calculate_positions(result, &byte_position, &bit_position);

		if ((bf->bitmap[byte_position] & (0x01 << bit_position)) == 0) {
			all_bits_set = false;
		}

		bf->bitmap[byte_position] |= (0x01 << bit_position);
	}

	if (all_bits_set == false) {
		bf->insertions += 1;
	}
}

/* bloom_add_string() - helper function for bloom_add() to handle strings
 *
 * Args:
 *     bf      - filter to use
 *     element - element to add to filter
 *
 * Returns:
 *     Nothing
 */
void bloom_add_string(bloomfilter *bf, const char *element) {
	bloom_add(bf, (uint8_t *)element, strlen(element));
}

/* bloom_lookup_or_add() - check if an element exists, adding it it doesn't
 *
 * Args:
 *     bf      - filter to perform lookup/add on
 *     element - element to lookup/add
 *     len     - length of element
 *
 * Returns:
 *     true if element is already in filter
 *     false if element was added to the filter
 *
 * TODO: test
 */
bool bloom_lookup_or_add(bloomfilter *bf, const void *element, const size_t len) {
	uint64_t hash[2];
	size_t   result;
	uint64_t byte_position;
	uint8_t  bit_position;
	bool     found_all = true;

	for (size_t i = 0; i < bf->hashcount; i++) {
		mmh3_128(element, len, i, hash);
		result = ((hash[0] % bf->size) + (hash[1] % bf->size)) % bf->size;

		calculate_positions(result, &byte_position, &bit_position);

		if ((bf->bitmap[byte_position] & (0x01 << bit_position)) == 0) {
			found_all = false;
			bf->bitmap[byte_position] |= (0x01 << bit_position);
		}
	}

	if (found_all == false) {
		bf->insertions += 1;
		return false; // element has been added to filter
	}

	return true; // element was already in filter
}

/* bloom_lookup_or_add_string() - helper function for bloom_lookup_or_add to
 *                                handle adding strings easier.
 *
 * Args:
 *     bf      - filter to lookup/add
 *     element - string element to add
 *
 * Returns:
 *     true if element was already in filter
 *     false if element was added to filter
 *
 * TODO: test
 */
bool bloom_lookup_or_add_string(bloomfilter *bf, const char *element) {
	return bloom_lookup_or_add(bf, element, strlen(element));
}

/* bloom_save() - save a bloom filter to disk
 *
 * Format of these files on disk is:
 *    +---------------------+
 *    | bloom filter struct |
 *    +---------------------+
 *    |        bitmap       |
 *    +---------------------+
 *
 * Args:
 *     bf   - filter to save
 *     path - file path to save filter
 *
 * Returns:
 *      BF_SUCCESS on success
 *      BF_FOPEN if unable to open file
 *      BF_FWRITE if unable to write to file
 *
 * TODO: test
 */
bloom_error_t bloom_save(const bloomfilter bf, const char *path) {
	FILE *fp;

	fp = fopen(path, "wb");
	if (fp == NULL) {
		return BF_FOPEN;
	}

	if (fwrite(&bf, sizeof(bloomfilter), 1, fp) != 1 ||
		fwrite(bf.bitmap, bf.bitmap_size, 1, fp) != 1) {
		fclose(fp);
		return BF_FWRITE;
	}

	fclose(fp);
	return BF_SUCCESS;
}

/* bloom_load() - load a bloom filter from disk
 *
 * Args:
 *     bf   - bloom filter object of new filter
 *     path - location of filter on disk
 *
 * Returns:
 *     BF_SUCCESS on success
 *     BF_FOPEN if unable to open file
 *     BF_FREAD if unable to read file
 *     BF_FSTAT if fstat() fails
 *     BF_INVALIDFILE if file is invalid
 *     BF_OUTOFMEMORY if memory allocation fails
 *
 * TODO: test various edge cases
 */
bloom_error_t bloom_load(bloomfilter *bf, const char *path) {
	FILE        *fp;
	struct stat  sb;

	fp = fopen(path, "rb");
	if (fp == NULL) {
		return BF_FOPEN;
	}

	if (fstat(fileno(fp), &sb) == -1) {
		fclose(fp);
		return BF_FSTAT;
	}

	if (fread(bf, sizeof(bloomfilter), 1, fp) != 1) {
		fclose(fp);
		return BF_FREAD;
	}

	// basic sanity check. should fail if filter isn't valid
	if ((bf->size / 8) != bf->bitmap_size ||
		sizeof(bloomfilter) + bf->bitmap_size != sb.st_size) {
		fclose(fp);
		return BF_INVALIDFILE;
	}

	bf->bitmap = malloc(bf->bitmap_size);
	if (bf->bitmap == NULL) {
		fclose(fp);
		return BF_OUTOFMEMORY;
	}

	if (fread(bf->bitmap, bf->bitmap_size, 1, fp) != 1) {
		fclose(fp);
		free(bf->bitmap);
		bf->bitmap = NULL;
		return BF_FREAD;
	}

	fclose(fp);

	return BF_SUCCESS;
}

/* bloom_strerror() - returns string containing error message
 *
 * Args:
 *     error - error number returned from function
 *
 * Returns:
 *     "Unknown error" if 'error' is out of range.
 *     Otherwise, a pointer to a string containing relevant error message.
 *
 * TODO test
 */
const char *bloom_strerror(bloom_error_t error) {
	if (error < 0 || error >= BF_ERRORCOUNT) {
		return "Unknown error";
	}

	return bloom_errors[error];
}
