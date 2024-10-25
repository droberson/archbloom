#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <errno.h>

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

	result = bloom_lookup_string(&bf, "foo");
	printf("foo: %d\n", result);
	if (result != true) {
		fprintf(stderr, "FAILURE: \"foo\" should be in filter\n");
		return EXIT_FAILURE;
	}

	result = bloom_lookup_string(&bf, "bar");
	printf("bar: %d\n", result);
	if (result != true) {
		fprintf(stderr, "FAILURE: \"bar\" should be in filter\n");
		return EXIT_FAILURE;
	}

	result = bloom_lookup_string(&bf, "baz");
	printf("baz: %d\n", result);
	if (result != false) {
		fprintf(stderr, "FAILURE: \"baz\" should NOT be in filter\n");
		return EXIT_FAILURE;
	}

	result =  bloom_lookup_string(&bf, "asdf");
	printf("asdf: %d\n", result);
	if (result != true) {
		fprintf(stderr, "FAILURE: \"asdf\" should be in filter\n");
		return EXIT_FAILURE;
	}

	// Hex dump the bitmap
	printf("filter hex dump: ");
	for (size_t i = 0; i < bf.bitmap_size; i++) {
		printf("%02x ", bf.bitmap[i]);
	}
	printf("\n");

	// Save to file
	char tmp_file_name[] = "/tmp/bloom.XXXXXX";
	int fd = mkstemp(tmp_file_name);
	if (fd == -1) {
		fprintf(stderr, "FAILURE: unable to create tmp file: %s -- %s\n",
				tmp_file_name,
				strerror(errno));
		return EXIT_FAILURE;
	}
	printf("attempting to save filter to %s\n", tmp_file_name);
	bloom_save_fd(&bf, fd);
	close(fd);
	bloom_destroy(&bf);

	// Load from file
	bloomfilter newbloom;
	bloom_load(&newbloom, tmp_file_name);

	printf("size: %d\n", newbloom.size);
	printf("hashcount: %d\n", newbloom.hashcount);
	printf("bitmap size: %d\n", newbloom.bitmap_size);

	printf("filter hex dump: ");
	for (size_t i = 0; i < newbloom.bitmap_size; i++) {
		printf("%02x ", newbloom.bitmap[i]);
	}
	printf("\n");

	result = bloom_lookup_string(&newbloom, "foo");
	printf("foo: %d\n", result);
	if (result != true) {
		fprintf(stderr, "FAILURE: \"foo\" should be in filter\n");
		return EXIT_FAILURE;
	}

	result = bloom_lookup_string(&newbloom, "bar");
	printf("bar: %d\n", result);
	if (result != true) {
		fprintf(stderr, "FAILURE: \"bar\" should be in filter\n");
		return EXIT_FAILURE;
	}

	result = bloom_lookup_string(&newbloom, "baz");
	printf("baz: %d\n", result);
	if (result != false) {
		fprintf(stderr, "FAILURE: \"baz\" should NOT be in filter\n");
		return EXIT_FAILURE;
	}

	result = bloom_lookup_string(&newbloom, "asdf");
	printf("asdf: %d\n", result);
	if (result != true) {
		fprintf(stderr, "FAILURE: \"asdf\" should be in filter\n");
		return EXIT_FAILURE;
	}

	// test clearing filter
	printf("testing clearing the filter\n");
	bloom_clear(&newbloom);
	result = bloom_lookup_string(&newbloom, "asdf");
	if (result != false) {
		fprintf(stderr, "FAILURE: \"asdf\" should NOT be in filter\n");
		return EXIT_FAILURE;
	}

	// test saturation count
	size_t saturation = bloom_saturation_count(&newbloom);
	printf("saturation count: %zd\n", saturation);
	if (saturation != 0) {
		fprintf(stderr, "FAILURE: saturation should be 0\n");
		return EXIT_FAILURE;
	}

	char buf[32];
	for (size_t i = 0; i < newbloom.expected / 2; i++) {
		snprintf(buf, sizeof(buf), "%zd", i);
		bloom_add_string(&newbloom, buf);
	}
	saturation = bloom_saturation_count(&newbloom);
	printf("half saturation: %zd/%zd\n", saturation, newbloom.size);

	for (size_t i = 0; i < newbloom.expected / 2; i++) {
		snprintf(buf, sizeof(buf), "%zd", i + (newbloom.expected / 2));
		bloom_add_string(&newbloom, buf);
	}
	saturation = bloom_saturation_count(&newbloom);
	printf("full saturation: %zd/%zd\n", saturation, newbloom.size);

	// Cleanup
	bloom_destroy(&newbloom);
	remove(tmp_file_name);

	return EXIT_SUCCESS;
}
