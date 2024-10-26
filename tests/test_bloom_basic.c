#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "bloom.h"


int main() {
	bloomfilter bf;

	puts("Initializing filter with 15 expected elements and 99.99% accuracy\n");
	bloom_init(&bf, 15, 0.01);
	printf("size: %d\n", bf.size);
	printf("hashcount: %d\n", bf.hashcount);
	printf("bitmap size: %d\n", bf.bitmap_size);

	// bloom_strerror() testing
	printf("testing bloom_strerror()\n");
	char *strerror_msg = (char *)bloom_strerror(BF_SUCCESS);
	if (strcmp("Success", strerror_msg) != 0) {
		fprintf(stderr, "FAILURE: %s != %s\n", "Success", strerror_msg);
		return EXIT_FAILURE;
	}

	strerror_msg = (char *)bloom_strerror(1000000);
	if (strcmp("Unknown error", strerror_msg) != 0) {
		fprintf(stderr, "FAILURE: %s != %s\n", "Unknown error", strerror_msg);
		return EXIT_FAILURE;
	}

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

	// bloom_lookup_or_add should return false if element isn't in filter
	// and true if element is already in filter.
	printf("testing bloom_lookup_or_add()\n");
	result = bloom_lookup_or_add_string(&bf, "asdf");
	if (result != true) {
		fprintf(stderr, "FAILURE: \"asdf\" should be in filter\n");
		return EXIT_FAILURE;
	}

	result = bloom_lookup_or_add_string(&bf, "asdfasdf");
	if (result != false) {
		fprintf(stderr, "FAILURE: \"asdfasdf\" should NOT have been in filter\n");
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

	// Load from file
	printf("attempting to load a file with bad permissions\n");
	bloomfilter bad_permissions;

	char *bad_permissions_file = "bloom-badperms";
	int bfd = open(bad_permissions_file, O_CREAT | O_WRONLY, 000);
	if (bfd == -1) {
		fprintf(stderr, "FAILURE: unable to create file %s: %s\n",
				bad_permissions_file,
				strerror(errno));
		return EXIT_FAILURE;
	}
	close(bfd);

	bloom_error_t badperm_error;
	badperm_error = bloom_load(&bad_permissions, bad_permissions_file);
	if (badperm_error != BF_FOPEN) {
		fprintf(stderr, "FAILURE: bloom_load() on bad permissions file\n");
		return EXIT_FAILURE;
	}
	bloom_destroy(&bad_permissions);
	remove(bad_permissions_file);

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

	bloom_add_string(&newbloom, "saturation should equal hashcount");
	saturation = bloom_saturation_count(&newbloom);
	printf("saturation after one record added: %d. hashcount %d\n",
		   saturation, newbloom.hashcount);
	if (saturation != newbloom.hashcount) {
		fprintf(stderr, "FAILURE: saturation should equal hashcount after one record has been added\n");
		return EXIT_FAILURE;
	}
	bloom_clear(&newbloom);

	char buf[32];
	for (size_t i = 0; i < newbloom.expected / 2; i++) {
		snprintf(buf, sizeof(buf), "%zd", i);
		bloom_add_string(&newbloom, buf);
	}
	saturation = bloom_saturation_count(&newbloom);
	float saturation_rate = (float)(saturation * 2) / (float)newbloom.size;
	printf("half saturation: (%zd * 2)/%zd = %f\n",
		   saturation,
		   newbloom.size,
		   saturation_rate);
	if (saturation_rate > 0.55 || saturation_rate < 0.40) {
		fprintf(stderr, "FAILURE: half saturation should be roughly 0.45\n");
		return EXIT_FAILURE;
	}

	for (size_t i = 0; i < newbloom.expected / 2; i++) {
		snprintf(buf, sizeof(buf), "%zd", i + (newbloom.expected / 2));
		bloom_add_string(&newbloom, buf);
	}
	saturation = bloom_saturation_count(&newbloom);
	saturation_rate = (float)(saturation * 2) / (float)newbloom.size;
	printf("full saturation: (%zd * 2)/%zd = %f\n",
		   saturation,
		   newbloom.size,
		   saturation_rate);
	if (saturation_rate > 0.90 || saturation_rate < 0.80) {
		fprintf(stderr, "FAILURE: full saturation should be roughly 0.90\n");
		return EXIT_FAILURE;
	}

	// test bloom_clear_if_saturation_exceeds()
	printf("bloom_saturation() = %f\n", bloom_saturation(&newbloom));
	result = bloom_clear_if_saturation_exceeds(&newbloom, 80.0);
	if (result != false) {
		fprintf(stderr, "FAILURE: bloom_clear_if_saturation_exceeds() shouldn't have succeeded\n");
		return EXIT_FAILURE;
	}

	result = bloom_clear_if_saturation_exceeds(&newbloom, 40.0);
	if (result != true) {
		fprintf(stderr, "FAILURE: bloom_clear_if_saturation_exceeds() should have succeeded\n");
		return EXIT_FAILURE;
	}

	// bloom_merge
	printf("testing bloom_merge()\n");
	bloomfilter m1, m2, merged_m, odd;

	bloom_init(&m1, 20, 0.01); // must be same characteristics to merge..
	bloom_init(&m2, 20, 0.01);
	bloom_init(&odd, 30, 0.1);  // different characteristics
	bloom_init(&merged_m, 20, 0.01);

	bloom_add_string(&m1, "one");
	bloom_add_string(&m1, "three");

	bloom_add_string(&m2, "two");
	bloom_add_string(&m2, "four");

	bloom_merge(&merged_m, &m1, &m2);

	result = bloom_lookup_string(&merged_m, "one");
	if (result != true) {
		fprintf(stderr, "FAILURE: \"one\" should be in merged filter\n");
		return EXIT_FAILURE;
	}

	result = bloom_lookup_string(&merged_m, "two");
	if (result != true) {
		fprintf(stderr, "FAILURE: \"two\" should be in merged filter\n");
		return EXIT_FAILURE;
	}

	result = bloom_lookup_string(&merged_m, "seven");
	if (result != false) {
		fprintf(stderr, "FAILURE: \"seven\" should NOT be in merged filter\n");
		return EXIT_FAILURE;
	}

	bloom_destroy(&merged_m);
	bloom_error_t merge_error;
	merge_error = bloom_merge(&merged_m, &m1, &odd); // should fail.
	if (merge_error != BF_INVALIDFILE) {
		fprintf(stderr, "FAILURE: bloom_merge() did not fail when merging incompatable filters\n");
		return EXIT_FAILURE;
	}

	bloom_destroy(&m1);
	bloom_destroy(&m2);
	bloom_destroy(&odd);
	bloom_destroy(&merged_m);

	// bloom_intersect(), bloom_estimate_intersection()
	printf("testing bloom_intersect()\n");
	bloomfilter i1, i2, intersected, oddball;
	bloom_error_t intersection_error;

	bloom_init(&i1, 25, 0.01);
	bloom_init(&i2, 25, 0.01);
	bloom_init(&intersected, 25, 0.01);
	bloom_init(&oddball, 40, 0.1);

	bloom_add_string(&i1, "common"); // should get intersected
	bloom_add_string(&i2, "common");
	bloom_add_string(&i1, "uncommon"); // shouldnt get intersected
	bloom_add_string(&i2, "strange");

	intersection_error = bloom_intersect(&intersected, &i1, &i2);
	if (intersection_error != BF_SUCCESS) {
		fprintf(stderr, "FAILURE: bloom_intersect() should have succeeded: %s\n", bloom_strerror(intersection_error));
		return EXIT_FAILURE;
	}

	float intersection_estimate = bloom_estimate_intersection(&i1, &i2);
	printf("intersection of i1 and i2: %f\n", intersection_estimate);
	if (intersection_estimate > 40.0 || intersection_estimate < 30.0) {
		fprintf(stderr, "FAILURE: intersection should be ~33%\n");
		return EXIT_FAILURE;
	}

	printf("i1:  ");
	for (size_t i = 0; i < i1.bitmap_size; i++) {
		printf("%02x ", i1.bitmap[i]);
	}
	printf("\n");
	printf("i2:  ");
	for (size_t i = 0; i < i2.bitmap_size; i++) {
		printf("%02x ", i2.bitmap[i]);
	}
	printf("\n");
		printf("res: ");
	for (size_t i = 0; i < intersected.bitmap_size; i++) {
		printf("%02x ", intersected.bitmap[i]);
	}
	printf("\n");

	result = bloom_lookup_string(&intersected, "common");
	if (result != true) {
		fprintf(stderr, "FAILURE: \"common\" should exist in intersected filter\n");
		return EXIT_FAILURE;
	}

	result = bloom_lookup_string(&intersected, "strange");
	if (result != false) {
		fprintf(stderr, "FAILURE: \"strange\" should NOT exist in intersected filter\n");
		return EXIT_FAILURE;
	}

	// Cleanup
	bloom_destroy(&newbloom);
	remove(tmp_file_name);

	return EXIT_SUCCESS;
}
