#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bloom.h"


int main() {
	bloomfilter bf;

	puts("Initializing filter with 15 expected elements and 99.99% accuracy\n");
	bloom_init(&bf, 15, 0.01);
	printf("size: %d\n", bf.size);
	printf("hashcount: %d\n", bf.hashcount);
	printf("bitmap size: %d\n", bf.bitmap_size);

	// add some elements to the bloom filter
	bloom_add(&bf, "asdf", strlen("asdf"));
	bloom_add_string(&bf, "bar");
	bloom_add_string(&bf, "foo");

	// Look up some stuff
	bool result;

	result = bloom_lookup_string(bf, "foo");
	printf("foo: %d\n", result);
	if (result != true) {
		fprintf(stderr, "FAILURE: \"foo\" should be in filter\n");
		return EXIT_FAILURE;
	}

	result = bloom_lookup_string(bf, "bar");
	printf("bar: %d\n", result);
	if (result != true) {
		fprintf(stderr, "FAILURE: \"bar\" should be in filter\n");
		return EXIT_FAILURE;
	}

	result = bloom_lookup_string(bf, "baz");
	printf("baz: %d\n", result);
	if (result != false) {
		fprintf(stderr, "FAILURE: \"baz\" should NOT be in filter\n");
		return EXIT_FAILURE;
	}

	result =  bloom_lookup_string(bf, "asdf");
	printf("asdf: %d\n", result);
	if (result != true) {
		fprintf(stderr, "FAILURE: \"asdf\" should be in filter\n");
		return EXIT_FAILURE;
	}

	// Hex dump the bitmap
	printf("occupancy: %lf %d\n", bloom_capacity(bf), bf.insertions);
	printf("filter hex dump: ");
	for (size_t i = 0; i < bf.bitmap_size; i++) {
		printf("%02x ", bf.bitmap[i]);
	}
	printf("\n");

	// Save to file
	// TODO: randomize /tmp filenames
	bloom_save(bf, "/tmp/bloom");
	bloom_destroy(&bf);

	// Load from file
	bloomfilter newbloom;
	bloom_load(&newbloom, "/tmp/bloom");

	printf("size: %d\n", newbloom.size);
	printf("hashcount: %d\n", newbloom.hashcount);
	printf("bitmap size: %d\n", newbloom.bitmap_size);

	printf("filter hex dump: ");
	for (size_t i = 0; i < newbloom.bitmap_size; i++) {
		printf("%02x ", newbloom.bitmap[i]);
	}
	printf("\n");

	result = bloom_lookup_string(newbloom, "foo");
	printf("foo: %d\n", result);
	if (result != true) {
		fprintf(stderr, "FAILURE: \"foo\" should be in filter\n");
		return EXIT_FAILURE;
	}

	result = bloom_lookup_string(newbloom, "bar");
	printf("bar: %d\n", result);
	if (result != true) {
		fprintf(stderr, "FAILURE: \"bar\" should be in filter\n");
		return EXIT_FAILURE;
	}

	result = bloom_lookup_string(newbloom, "baz");
	printf("baz: %d\n", result);
	if (result != false) {
		fprintf(stderr, "FAILURE: \"baz\" should NOT be in filter\n");
		return EXIT_FAILURE;
	}

	result = bloom_lookup_string(newbloom, "asdf");
	printf("asdf: %d\n", result);
	if (result != true) {
		fprintf(stderr, "FAILURE: \"asdf\" should be in filter\n");
		return EXIT_FAILURE;
	}

	// Cleanup
	bloom_destroy(&newbloom);

	remove("/tmp/bloom");

	return EXIT_SUCCESS;
}
