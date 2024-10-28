/* test_cbloom_basic.c -- tests for counting bloom filters
 * TODO: split tests into more atomic tests wherever possible instead of one
 *       test program for everything.
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "cbloom.h"

int main() {
	cbloomfilter cbf;
	bool                result;

	printf("initializing counting bloom filter expecting 20 elements\n");
	cbloom_error_t init_result = cbloom_init(&cbf, 20, 0.01, COUNTER_8BIT);
	if (init_result != CBF_SUCCESS) {
		fprintf(stderr, "FAILURE: failed creating 8 bit filter: %s\n",
				cbloom_strerror(init_result));
		return EXIT_FAILURE;
	}

	printf("\tsize: %d\n", cbf.size);
	printf("\thash count: %d\n", cbf.hashcount);
	printf("\tcountermap size: %d\n", cbf.countermap_size);
	printf("\tcounter size (bits): %d\n", (size_t)pow(2, (cbf.csize + 3)));

	// add some data
	cbloom_add_string(&cbf, "foo");
	cbloom_add_string(&cbf, "bar");
	// add a string twice to test removal
	cbloom_add_string(&cbf, "multi");
	cbloom_add_string(&cbf, "multi");

	result = cbloom_lookup_string(&cbf, "foo");
	printf("cbf foo lookup: %d\n", result);
	if (result != true) {
		fprintf(stderr, "FAILURE: \"foo\" should be in filter\n");
		return EXIT_FAILURE;
	}

	result = cbloom_lookup_string(&cbf, "bar");
	printf("cbf bar lookup: %d\n", result);
	if (result != true) {
		fprintf(stderr, "FAILURE: \"bar\" should be in filter\n");
		return EXIT_FAILURE;
	}

	result = cbloom_lookup_string(&cbf, "baz");
	printf("cbf baz lookup: %d\n", result);
	if (result != false) {
		fprintf(stderr, "FAILURE: \"bar\" should NOT be in filter\n");
		return EXIT_FAILURE;
	}

	size_t count = cbloom_count_string(&cbf, "multi");
	printf("count \"multi\": %d\n", count);
	if (count != 2) {
		fprintf(stderr, "FAILURE: count should be 2\n");
		return EXIT_FAILURE;
	}

	count = cbloom_count_string(&cbf, "fizzbuzz");
	printf("count \"fizzbuzz\": %d\n", count);
	if (count != 0) {
		fprintf(stderr, "FAILURE: count should be 0\n");
		return EXIT_FAILURE;
	}

	// test removal
	printf("removing elements from filter\n");
	cbloom_remove_string(&cbf, "bar");
	cbloom_remove_string(&cbf, "multi");

	result = cbloom_lookup_string(&cbf, "bar");
	printf("cbf bar lookup: %d\n", result);
	if (result != false) {
		fprintf(stderr, "FAILURE: \"bar\" should NOT be in filter\n");
		return EXIT_FAILURE;
	}

	result = cbloom_lookup_string(&cbf, "multi");
	printf("cbf multi lookup: %d\n", result);
	if (result != true) {
		fprintf(stderr, "FAILURE: \"multi\" should be in filter\n");
		return EXIT_FAILURE;
	}

	// TODO test behavior if file doesn't exist or cant be written to
	printf("Testing saving and loading from disk\n");
	cbloom_error_t save_result = cbloom_save(&cbf, "/tmp/cbloom");
	if (save_result != CBF_SUCCESS) {
		fprintf(stderr, "FAILURE: failed to save 8 bit counting bloom filter file to /tmp/cbloom: %s\n", cbloom_strerror(save_result));
		return EXIT_FAILURE;
	}

	cbloomfilter newcbf;
	// TODO test if file cannot be open or read.
	// TODO test if file is invalid
	cbloom_load(&newcbf, "/tmp/cbloom");
	printf("\tsize: %d\n", newcbf.size);
	printf("\thash count: %d\n", newcbf.hashcount);
	printf("\tcountermap size: %d\n", newcbf.countermap_size);
	printf("\tcounter size (bits): %d\n", (size_t)pow(2, (newcbf.csize + 3)));

	result = cbloom_lookup_string(&newcbf, "multi");
	printf("cbf multi lookup: %d\n", result);
	if (result != true) {
		fprintf(stderr, "FAILURE: \"multi\" should be in filter\n");
		return EXIT_FAILURE;
	}

	result = cbloom_lookup_string(&newcbf, "bar");
	printf("cbf bar lookup: %d\n", result);
	if (result != false) {
		fprintf(stderr, "FAILURE: \"bar\" should NOT be in filter\n");
		return EXIT_FAILURE;
	}

	// test creation of 4, 16, 32, 64 bit filters
	cbloomfilter cbf4;
	cbloomfilter cbf16;
	cbloomfilter cbf32;
	cbloomfilter cbf64;

	init_result = cbloom_init(&cbf4, 20, 0.01, COUNTER_4BIT);
	if (init_result != CBF_SUCCESS) {
		fprintf(stderr, "FAILURE: creation of 4 bit counter: %s\n",
				cbloom_strerror(init_result));
		return EXIT_FAILURE;
	}
	printf("4 bit:\n");
	printf("\tsize: %d\n", cbf4.size);
	printf("\thash count: %d\n", cbf4.hashcount);
	printf("\tcountermap size: %d\n", cbf4.countermap_size);
	printf("\tcounter size (bits): %d\n", (size_t)pow(2, (cbf4.csize + 3)) / 2);
	cbloom_destroy(&cbf4);

	// 16 bit
	init_result = cbloom_init(&cbf16, 20, 0.01, COUNTER_16BIT);
	if (init_result != CBF_SUCCESS) {
		fprintf(stderr, "FAILURE: creation of 16 bit counter: %s\n",
				cbloom_strerror(init_result));
		return EXIT_FAILURE;
	}
	printf("16 bit:\n");
	printf("\tsize: %d\n", cbf16.size);
	printf("\thash count: %d\n", cbf16.hashcount);
	printf("\tcountermap size: %d\n", cbf16.countermap_size);
	printf("\tcounter size (bits): %d\n", (size_t)pow(2, (cbf16.csize + 3)) / 2);
	cbloom_destroy(&cbf16);

	// 32 bit
	init_result = cbloom_init(&cbf32, 20, 0.01, COUNTER_32BIT);
	if (init_result != CBF_SUCCESS) {
		fprintf(stderr, "FAILURE: failed creating 32 bit filter: %s\n",
				cbloom_strerror(init_result));
		return EXIT_FAILURE;
	}

	printf("32 bit:\n");
	printf("\tsize: %d\n", cbf32.size);
	printf("\thash count: %d\n", cbf32.hashcount);
	printf("\tcountermap size: %d\n", cbf32.countermap_size);
	printf("\tcounter size (bits): %d\n", (size_t)pow(2, (cbf32.csize + 3)) / 2);

	cbloom_add_string(&cbf32, "the last metroid is in captivity");
	cbloom_add_string(&cbf32, "the galaxy is at peace.");
	cbloom_add_string(&cbf32, "blap");
	cbloom_add_string(&cbf32, "blap");

	result = cbloom_lookup_string(&cbf32, "the last metroid is in captivity");
	if (result != true) {
		fprintf(stderr, "FAILURE: \"the last metroid is in captivity\" should be in filter\n");
		return EXIT_FAILURE;
	}

	result = cbloom_save(&cbf32, "/tmp/cbf32");
	if (result != CBF_SUCCESS) {
		fprintf(stderr, "FAILURE: unable to save 32 bit counter to disk\n");
		return EXIT_FAILURE;
	}
	cbloom_destroy(&cbf32);
	result = cbloom_load(&cbf32, "/tmp/cbf32");
	if (result != CBF_SUCCESS) {
		fprintf(stderr, "FAILURE: unable to load 32 bit filter from disk\n");
		return EXIT_FAILURE;
	}
	printf("32 bit loaded from disk:\n");
	printf("\tsize: %d\n", cbf32.size);
	printf("\thash count: %d\n", cbf32.hashcount);
	printf("\tcountermap size: %d\n", cbf32.countermap_size);
	printf("\tcounter size (bits): %d\n", (size_t)pow(2, (cbf32.csize + 3)) / 2);

	result = cbloom_lookup_string(&cbf32, "the last metroid is in captivity");
	if (result != true) {
		fprintf(stderr, "FAILURE: \"the last metroid is in captivity\" should be in filter\n");
		return EXIT_FAILURE;
	}

	// 64 bit
	init_result = cbloom_init(&cbf64, 20, 0.01, COUNTER_64BIT);
	if (init_result != CBF_SUCCESS) {
		fprintf(stderr, "FAILURE: failed creating 64 bit filter: %s\n",
				cbloom_strerror(init_result));
		return EXIT_FAILURE;
	}

	printf("64 bit:\n");
	printf("\tsize: %d\n", cbf64.size);
	printf("\thash count: %d\n", cbf64.hashcount);
	printf("\tcountermap size: %d\n", cbf64.countermap_size);
	printf("\tcounter size (bits): %d\n", (size_t)pow(2, (cbf64.csize + 3)) / 2);
	cbloom_destroy(&cbf64);

	// cleanup
	// TODO: make random tmp files instead of hard-coded.
	remove("/tmp/cbloom");
	remove("/tmp/cbf32");

	cbloom_destroy(&cbf);
	cbloom_destroy(&newcbf);

	return EXIT_SUCCESS;
}
