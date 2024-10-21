/* bloom.c
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

/* bloom_init() -- initialize a bloom filter
 *
 * Args:
 *     bf       - bloomfilter structure
 *     expected - expected number of elements
 *     accuracy - margin of acceptable error. ex: 0.01 is "99.99%" accurate
 *
 * Returns:
 *     true on success, false on failure
 */
bool bloom_init(bloomfilter *bf, const size_t expected, const float accuracy) {
	bf->size        = ideal_size(expected, accuracy);
	bf->hashcount   = (bf->size / expected) * log(2);
	bf->bitmap_size = ceil(bf->size / 8);
	bf->expected    = expected;
	bf->accuracy    = accuracy;
	bf->insertions  = 0;

	bf->bitmap      = calloc(bf->bitmap_size, sizeof(uint8_t));
	if (bf->bitmap == NULL) {
		return false;
	}

	return true;
}

/* bloom_destroy() -- free a bloom filter's allocated memory
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

/* bloom_capacity() -- returns the occupancy of a bloom filter as a percentage
 *
 * Args:
 *     bf - filter to check capacity
 *
 * Returns:
 *     a double representing the occupancy of the bloom filter
 */
double bloom_capacity(bloomfilter bf) {
	return ((double)bf.insertions / (double)bf.expected) * 100.0;
}

/* bloom_lookup() -- check if an element is likely in a filter
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
bool bloom_lookup(const bloomfilter bf, void *element, const size_t len) {
	uint64_t hash[2];
	uint64_t result;
	uint64_t bytepos;
	uint8_t  bitpos;

	for (int i = 0; i < bf.hashcount; i++) {
		mmh3_128(element, len, i, hash);
		result = ((hash[0] % bf.size) + (hash[1] % bf.size)) % bf.size;

		bytepos = ceil(result / 8);
		bitpos = result % 8;

		if ((bf.bitmap[bytepos] & (0x01 << bitpos)) == 0) {
			return false;
		}
	}

	return true;
}

/* bloom_lookup_string() -- helper function for bloom_lookup() to handle strings
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

/* bloom_add() -- add/insert an element into a bloom filter
 *
 * Args:
 *     bf      - filter to use
 *     element - element to add
 *     len     - element length in bytes
 *
 * Returns:
 *     Nothing
 */
void bloom_add(bloomfilter *bf, void *element, const size_t len) {
	uint64_t  hash[2];
	uint64_t  result;
	uint64_t  byte_position;
	uint64_t  bit_position;
	bool      all_bits_set = true;

	for (int i = 0; i < bf->hashcount; i++) {
		mmh3_128(element, len, i, hash);
		result = ((hash[0] % bf->size) + (hash[1] % bf->size)) % bf->size;

		byte_position = ceil(result / 8);
		bit_position  = result % 8;

		if ((bf->bitmap[byte_position] & (0x01 << bit_position)) == 0) {
			all_bits_set = false;
		}

		bf->bitmap[byte_position] |= (0x01 << bit_position);
	}

	if (all_bits_set == false) {
		bf->insertions += 1;
	}
}

/* bloom_add_string() -- helper function for bloom_add() to handle strings
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

/* bloom_save() -- save a bloom filter to disk
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
 *      true on success, false on failure
 */
bool bloom_save(bloomfilter bf, const char *path) {
	FILE *fp;

	fp = fopen(path, "wb");
	if (fp == NULL) {
		return false;
	}

	if (fwrite(&bf, sizeof(bloomfilter), 1, fp) != 1 ||
		fwrite(bf.bitmap, bf.bitmap_size, 1, fp) != 1) {
		fclose(fp);
		return false;
	}

	fclose(fp);
	return true;
}

/* bloom_load() -- load a bloom filter from disk
 *
 * Args:
 *     bf   - bloom filter object of new filter
 *     path - location of filter on disk
 *
 * Returns:
 *     true on success, false on failure
 */
bool bloom_load(bloomfilter *bf, const char *path) {
	FILE        *fp;
	struct stat  sb;

	fp = fopen(path, "rb");
	if (fp == NULL) {
		return false;
	}

	if (fstat(fileno(fp), &sb) == -1) {
		fclose(fp);
		return false;
	}

	fread(bf, sizeof(bloomfilter), 1, fp);

	// basic sanity check. should fail if filter isn't valid
	if (ceil(bf->size / 8) != bf->bitmap_size ||
		sizeof(bloomfilter) + bf->bitmap_size != sb.st_size) {
		fclose(fp);
		return false;
	}

	bf->bitmap = malloc(bf->bitmap_size);
	if (bf->bitmap == NULL) {
		fclose(fp);
		return false;
	}

	fread(bf->bitmap, bf->bitmap_size, 1, fp);

	fclose(fp);

	return true;
}
