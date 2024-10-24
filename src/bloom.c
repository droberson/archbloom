/**
 * @file bloom.c
 * @brief Bloom filter implementation.
 *
 * This file contains functions for working with Bloom filters,
 * including initialization, destruction, insertion, querying, and
 * saving and loading filters from disk.
 *
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
static size_t ideal_size(const size_t expected, const float accuracy) {
	return -(expected * log(accuracy) / pow(log(2.0), 2));
}

/**
 * @brief Initialize a Bloom filter
 *
 * This function initializes a Bloom filter based on the expected number of
 * elements it will contain and the desired accuracy.
 *
 * @param bf       Pointer to a bloomfilter structure.
 * @param expected Expected number of elements the filter will contain.
 * @param accuracy Margin of acceptable error. ex: 0.01 is "99.99%" accurate.
 *
 * @return BF_SUCCESS on successful initialization.
 * @return BF_OUTOFMEMORY if memory allocation fails.
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

/**
 * @brief Free the memory allocated for a Bloom filter.
 *
 * This function frees memory associated with a the given Bloom filter.
 *
 * @param bf Pointer to the Bloom filter to free.
 */
void bloom_destroy(bloomfilter *bf) {
	if (bf->bitmap) {
		free(bf->bitmap);
		bf->bitmap = NULL;
	}
}

/**
 * @brief Clear the contents of a Bloom filter and it's insertion counter.
 *
 * This function empties the Bloom filter and resets the counter that
 * tracks the number of insertions performed on the filter.
 *
 * @param bf Pointer to the Bloom filter to clear.
 *
 * TODO: test
 */
void bloom_clear(bloomfilter *bf) {
	memset(bf->bitmap, 0, bf->bitmap_size);
	bf->insertions = 0;
}

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

/**
 * @brief Calculate the number of bits set to 1 in a Bloom filter.
 *
 * This function counts the number of bits set to 1 in a Bloom
 * filter. This calculation is useful for determining how full the
 * filter is.
 *
 * @param bf Bloom filter to count.
 *
 * @return The number of bits set to 1 in the provided Bloom filter.
 * TODO: test
 */
size_t bloom_saturation_count(const bloomfilter *bf) {
	size_t count = 0;

	for (size_t i = 0; i < bf->bitmap_size; i++) {
		count += bit_count_table[bf->bitmap[i]];
	}

	return count;
}

/**
 * @brief Calculate the saturation of a Bloom filter (the percentage
 * of bits set).
 *
 * This function computes the percentage of bits set to 1 in a
 * provided Bloom filter, which gives an estimate of how full a filter
 * is.
 *
 * @param bf Bloom filter to calculate saturation for.
 *
 * @return The percentage of bits set in filter as a floating-point value.
 *
 * TODO: test
 */
float bloom_saturation(const bloomfilter *bf) {
	size_t total_bits = bf->bitmap_size * 8;
	size_t set_bits   = bloom_saturation_count(bf);

	return (float)set_bits / total_bits * 100.0;
}

/**
 * @brief Clears the Bloom filter if its saturation exceeds the given threshold.
 *
 * This function checks the saturation of the Bloom filter and, if it
 * exceeds the specified threshold, clears the filter and returns
 * `true`. If the saturation is under the threshold, the filter
 * remains unchanged and the function returns `false`.
 *
 * @param bf Pointer to the Bloom filter structure.
 * @param threshold The saturation threshold (eg: 0.01 - 99.99).
 *
 * @return true if the filter was cleared,
 * @return false if filter was not cleared (filter was under the
 * saturation threshold).
 *
 * TODO: test
 */
bool bloom_clear_if_saturation_exceeds(bloomfilter *bf, float threshold) {
    float saturation = bloom_saturation(bf);

    if (saturation > threshold) {
        bloom_clear(bf);
        return true;
    }

    return false;
}

/**
 * @brief Estimate the false positive rate of a Bloom filter.
 *
 * This function calculates the estimated false positive rate of the
 * given Bloom filter.
 *
 * where:
 * - `k` is the number of hash functions used (`bf->hashcount`),
 * - `n` is the number of elements inserted (determined by
 *    `bloom_saturation_count()` or other methods),
 * - `m` is the size of the bit array in the Bloom filter (`bf->size`).
 *
 * The false positive rate represents the likelihood that a query for
 * an element that is not in the set will incorrectly return true.
 *
 * @param[in] bf Pointer to the Bloom filter structure.
 *
 * @return The estimated false positive rate as a floating-point
 * number between 0 and 1.
 */
float bloom_estimate_false_positive_rate(const bloomfilter *bf) {
	/*
	 * false positive rate formula: (1 - exp(-kn/m)) ^ k
	 *
	 * where:
	 *     k = hash count
	 *     n = number of elements
	 *     m = size of filter
	 */
	size_t m = bf->size;
	size_t n = bloom_saturation_count(bf); // TODO: use insertions instead?
	size_t k = bf->hashcount;

	float exp_factor = exp(-(float)k * (float)n / (float)m);
	float fp_rate = pow(1.0f - exp_factor, (float)k);

	return fp_rate;
}

/**
 * @brief Calculate the capacity of a bloom filter as a percentage
 * based on the expected number of elements and number of insertions.
 *
 * This function returns the used capacity of the Bloom filter based
 * on the number of insertions and expected number of elements,
 * expressed as a percentage.
 *
 * @param bf Bloom filter to check capacity of.
 *
 * @return A double representing the used capacity of the bloom filter.
 *
 * TODO: is this function worthwhile? seems like saturation is a better measure
 */
double bloom_capacity(const bloomfilter *bf) {
	return ((double)bf->insertions / (double)bf->expected) * 100.0;
}

/**
 * @brief Helper function to get byte and bit positions of elements in
 * a Bloom filter.
 *
 * @param result Position of element.
 * @param byte_position Pointer to store calculated byte position.
 * @param bit_position Pointer to store bit calculated bit position.
 *
 * TODO: rename 'result' parameter?
 */
static inline void calculate_positions(uint64_t position, uint64_t *byte_position, uint8_t *bit_position) {
	*byte_position = position / 8;
	*bit_position = position % 8;
}

/**
 * @brief Estimate the overlap (intersection) between two Bloom filters.
 *
 * This function calculates the ratio of bits set to 1 in both Bloom
 * filters, providing an estimate of how much their sets overlap. The
 * result is the percentage of bits set in both filters, giving an
 * idea of the overlap between the two sets represented by the Bloom
 * filters.
 *
 * The two Bloom filters must have the same size, hash count, and accuracy.
 *
 * @param bf1 Pointer to the first Bloom filter.
 * @param bf2 Pointer to the second Bloom filter.
 *
 * @return A percentage between 0.0 and 99.999, representing the
 *         estimated overlap (intersection) between the two Bloom
 *         filters.
 * @return -1.0f on failure (Bloom filters aren't compatible for this
 *         operation).
 *
 * TODO: test
 */
float bloom_estimate_intersection(const bloomfilter *bf1, const bloomfilter *bf2) {
	if (bf1->size != bf2->size ||
		bf1->hashcount != bf2->hashcount) {
		return -1.0f; // error.
	}

	size_t intersection_count = 0;
	size_t bits_total = bf1->bitmap_size * 8;

	for (size_t i = 0; i < bf1->bitmap_size; i++) {
		uint8_t intersection_bits = bf1->bitmap[i] & bf2->bitmap[i];
		intersection_count += bit_count_table[intersection_bits];
	}

	return ((float)intersection_count / bits_total) * 100.0;
}

/**
 * @brief Check if an element is likely present in a Bloom filter.
 *
 * This function determined whether or not an element is probably in
 * the filter or definitely not in the filter.
 *
 * @param bf Bloom filter to perform look up against.
 * @param element Pointer to the element to look up.
 * @param len Length of the element in bytes.
 *
 * @return true if the element is probably in the filter.
 * @return false if the element is definitely not in the filter.
 */
bool bloom_lookup(const bloomfilter *bf, const void *element, const size_t len) {
	uint64_t hashes[bf->hashcount];
	uint64_t result;
	uint64_t byte_position;
	uint8_t  bit_position;

	mmh3_64_make_hashes(element, len, bf->hashcount, hashes);

	for (size_t i = 0; i < bf->hashcount; i++) {
		result = hashes[i] % bf->size;

		calculate_positions(result, &byte_position, &bit_position);

		if ((bf->bitmap[byte_position] & (0x01 << bit_position)) == 0) {
			return false;
		}
	}

	return true;
}

/**
 * @brief Helper function for bloom_lookup() to handle string elements.
 *
 * This function is a convenience wrapper for `bloom_lookup()` specifically
 * for handling string elements.
 *
 * @param bf Bloom filter to perform look up against.
 * @param element Pointer to the string element to look up.
 *
 * @return true if the string is likely in the filter.
 * @return false if the element is definitely not in the filter.
 */
bool bloom_lookup_string(const bloomfilter *bf, const char *element) {
	return bloom_lookup(bf, (uint8_t *)element, strlen(element));
}

/**
 * @brief Add or insert an element into a Bloom filter.
 *
 * This function inserts an element into a Bloom filter.
 *
 * @param bf Bloom filter to add element to.
 * @param element Pointer to element to add.
 * @param len Length of element in bytes.
 */
void bloom_add(bloomfilter *bf, const void *element, const size_t len) {
	uint64_t  hashes[bf->hashcount];
	uint64_t  result;
	uint64_t  byte_position;
	uint8_t   bit_position;
	bool      all_bits_set = true;

	mmh3_64_make_hashes(element, len, bf->hashcount, hashes);

	for (size_t i = 0; i < bf->hashcount; i++) {
		result = hashes[i] % bf->size;

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

/**
 * @brief Helper function for `bloom_add()` to handle string elements.
 *
 * This function is a convenience wrapper around `bloom_add()`
 * specifically for adding string elements to a Bloom filter.
 *
 * @param bf Bloom filter to add a string element to.
 * @param element Pointer to the string element to add to the filter.
 */
void bloom_add_string(bloomfilter *bf, const char *element) {
	bloom_add(bf, (uint8_t *)element, strlen(element));
}

/**
 * @brief Check if an element exists in a Bloom filter, adding it if
 * it does not exist.
 *
 * @param bf Pointer to the Bloom filter to perform look up or add.
 * @param element Pointer to the element to look up or add.
 * @param len Length of the element in bytes
 *
 * @return true if the element is already in the filter.
 * @return false if the element was added to the filter
 *
 * TODO: test
 */
bool bloom_lookup_or_add(bloomfilter *bf, const void *element, const size_t len) {
	uint64_t hashes[bf->hashcount];
	size_t   result;
	uint64_t byte_position;
	uint8_t  bit_position;
	bool     found_all = true;

	mmh3_64_make_hashes(element, len, bf->hashcount, hashes);

	for (size_t i = 0; i < bf->hashcount; i++) {
		result = hashes[i] % bf->size;

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

/**
 * @brief Helper function for bloom_lookup_or_add() to handle string elements.
 *
 * This function is a convenience wrapper around
 * `bloom_lookup_or_add()` that makes it easier to work with string
 * elements.
 *
 * @param bf Pointer to the Bloom filter to perform this operation against..
 * @param element Pointer to the string element to check or add.
 *
 * @return true if the string was already in the filter.
 * @return false if the string was newly added.
 *
 * TODO: test
 */
bool bloom_lookup_or_add_string(bloomfilter *bf, const char *element) {
	return bloom_lookup_or_add(bf, element, strlen(element));
}

/**
 * @brief Add an element to the Bloom filter only if it is not already present.
 *
 * This function checks if an element is already present in the Bloom filter.
 * If the element is not present, it adds the element and increments the
 * insertion counter.
 *
 * @param bf Pointer to the Bloom filter.
 * @param element Pointer to the element to add.
 * @param len Length of the element in bytes.
 *
 * @return true if the element was already present.
 * @return false if the element was added to the filter.
 */
bool bloom_add_if_not_present(bloomfilter *bf, const void *element, const size_t len) {
    if (bloom_lookup(bf, element, len)) {
        return true;
    }

    bloom_add(bf, element, len);
    return false;
}

/**
 * @brief Add a string element to the Bloom filter only if it is not
 * already present.
 *
 * This function checks if a string element is already present in the
 * Bloom filter. If the string element is not present, it adds the
 * element and increments the insertion counter.
 *
 * This function is a convenience wrapper around
 * `bloom_add_if_not_present()` that handles string elements.
 *
 * @param bf Pointer to the Bloom filter.
 * @param element Pointer to the string element to add.
 *
 * @return true if the string element was already present.
 * @return false if the string element was added to the filter.
 *
 * @see bloom_add_if_not_present
 */
bool bloom_add_if_not_present_string(bloomfilter *bf, const char *element) {
	return bloom_add_if_not_present(bf, element, strlen(element));
}

/**
 * @brief Saves a Bloom filter to disk.
 *
 * This function saves the Bloom filter to a file on disk. The file
 * contains two sections:
 *
 * 1. The Bloom filter structure.
 * 2. The bitmap data.
 *
 * @param bf Bloom filter to save to disk.
 * @param path File path where the Bloom filter will be saved.
 *
 * @return BF_SUCCESS on success.
 * @return BF_FOPEN if unable to open the file.
 * @return BF_FWRITE if unable to write to the file.
 *
 * TODO: test
 */
bloom_error_t bloom_save(const bloomfilter *bf, const char *path) {
	FILE *fp;

	fp = fopen(path, "wb");
	if (fp == NULL) {
		return BF_FOPEN;
	}

	if (fwrite(bf, sizeof(bloomfilter), 1, fp) != 1 ||
		fwrite(bf->bitmap, bf->bitmap_size, 1, fp) != 1) {
		fclose(fp);
		return BF_FWRITE;
	}

	fclose(fp);
	return BF_SUCCESS;
}

/**
 * @brief Load a Bloom filter from a file on disk.
 *
 * This function reads a Bloom filter from the specified file and
 * initializes the Bloom filter object.
 *
 * @param bf Pointer to the Bloom filter object to initialize.
 * @param path File path from which to load the Bloom filter.
 *
 * @return BF_SUCCESS on success.
 * @return BF_FOPEN if unable to open the file.
 * @return BF_FREAD if unable to read the file.
 * @return BF_FSTAT if fstat() fails.
 * @return BF_INVALIDFILE if the file is invalid.
 * @return BF_OUTOFMEMORY if memory allocation fails.
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

/**
 * @brief Return a string containing the error message corresponding
 * to an error code.
 *
 * This function converts an error code returned by a Bloom filter
 * function into a human-readable error message.
 *
 * @param error The error code returned by a Bloom filter function.
 *
 * @return A pointer to a string containing the relevant error
 * message, or "Unknown error" if the error code is out of range.
 *
 * TODO test
 */
const char *bloom_strerror(bloom_error_t error) {
	if (error < 0 || error >= BF_ERRORCOUNT) {
		return "Unknown error";
	}

	return bloom_errors[error];
}

/**
 * @brief Merge two Bloom filters into a result filter.
 *
 * This function combines two Bloom filters by taking the logical OR
 * of their bitmaps. The two Bloom filters must have the same size,
 * hashcount, and accuracy to be merged.
 *
 * @param result A pointer to the Bloom filter to store the merged result.
 * @param bf1 A pointer to the first Bloom filter.
 * @param bf2 A pointer to the second Bloom filter.
 *
 * @return BF_SUCCESS on successful merge.
 * @return BF_OUTOFMEMORY if memory allocation fails for the result bitmap.
 * @return BF_INVALIDFILE if the two Bloom filters are not compatible.
 *
 * TODO: test
 */
bloom_error_t bloom_merge(bloomfilter *result,
						  const bloomfilter *bf1,
						  const bloomfilter *bf2) {
    if (bf1->size != bf2->size ||
		bf1->hashcount != bf2->hashcount ||
		bf1->accuracy != bf2->accuracy) {
        return BF_INVALIDFILE;
    }

    result->size        = bf1->size;
    result->hashcount   = bf1->hashcount;
    result->accuracy    = bf1->accuracy;
    result->bitmap_size = bf1->bitmap_size;
    result->expected    = bf1->expected;
    result->insertions  = bf1->insertions;

    result->bitmap = calloc(result->bitmap_size, sizeof(uint8_t));
    if (result->bitmap == NULL) {
        return BF_OUTOFMEMORY;
    }

    for (size_t i = 0; i < result->bitmap_size; i++) {
        result->bitmap[i] = bf1->bitmap[i] | bf2->bitmap[i];
    }

    result->insertions += bf2->insertions; // TODO consider removing insertions

    return BF_SUCCESS;
}

/**
 * @brief Intersect two Bloom filters into a result filter.
 *
 * This function combines two Bloom filters by taking the logical AND
 * of their bitmaps. The two Bloom filters must have the same size,
 * hashcount, and accuracy to be intersected.
 *
 * @param result A pointer to the Bloom filter to store the intersected result.
 * @param bf1 A pointer to the first Bloom filter.
 * @param bf2 A pointer to the second Bloom filter.
 *
 * @return BF_SUCCESS on successful merge.
 * @return BF_OUTOFMEMORY if memory allocation fails for the result bitmap.
 * @return BF_INVALIDFILE if the two Bloom filters are not compatible.
 *
 * TODO: test
 */
bloom_error_t bloom_intersect(bloomfilter *result,
							  const bloomfilter *bf1,
							  const bloomfilter *bf2) {
    if (bf1->size != bf2->size ||
		bf1->hashcount != bf2->hashcount ||
		bf1->accuracy != bf2->accuracy) {
        return BF_INVALIDFILE;
    }

    result->size        = bf1->size;
    result->hashcount   = bf1->hashcount;
    result->accuracy    = bf1->accuracy;
    result->bitmap_size = bf1->bitmap_size;
    result->expected    = bf1->expected;
    result->insertions  = bf1->insertions;

    result->bitmap = calloc(result->bitmap_size, sizeof(uint8_t));
    if (result->bitmap == NULL) {
        return BF_OUTOFMEMORY;
    }

    for (size_t i = 0; i < result->bitmap_size; i++) {
        result->bitmap[i] = bf1->bitmap[i] & bf2->bitmap[i];
    }

    result->insertions += bf2->insertions; // TODO consider removing insertions

    return BF_SUCCESS;
}
