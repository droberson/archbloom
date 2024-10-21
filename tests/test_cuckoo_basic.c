#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cuckoo.h"

int main() {
	cuckoofilter cf;

	printf("initializing cuckoo filter\n");
	cuckoo_init(&cf, 1000, 4, 500);

	// add elements to filter
	cuckoo_add(cf, "foo", strlen("foo"));
	cuckoo_add(cf, "bar", strlen("bar"));

	bool result;
	result = cuckoo_lookup(cf, "foo", strlen("foo"));
	printf("cuckoo foo lookup: %d\n", result);
	if (result != true) {
		fprintf(stderr, "FATAL: \"foo\" should be in filter\n");
		return EXIT_FAILURE;
	}

	result = cuckoo_lookup(cf, "bar", strlen("bar"));
	printf("cuckoo bar lookup: %d\n", result);
	if (result != true) {
		fprintf(stderr, "FATAL: \"foo\" should be in filter\n");
		return EXIT_FAILURE;
	}

	result = cuckoo_lookup(cf, "baz", strlen("baz"));
	printf("cuckoo baz lookup: %d\n", result);
	if (result != false) {
		fprintf(stderr, "FATAL: \"baz\" should be in filter\n");
		return EXIT_FAILURE;
	}

	// test removal
	cuckoo_remove(cf, "foo", strlen("foo"));
	result = cuckoo_lookup(cf, "foo", strlen("foo"));
	printf("cuckoo foo lookup: %d\n", result);
	if (result != false) {
		fprintf(stderr, "FATAL: \"foo\" should NOT be in filter\n");
		return EXIT_FAILURE;
	}

	// test file save/load
	printf("testing saving/loading\n");
	cuckoo_add_string(cf, "beep");
	cuckoo_add_string(cf, "boop");

	printf("saving old filter to /tmp/cuckoo\n");
	cuckoo_save(cf, "/tmp/cuckoo");

	cuckoofilter newcf;

	printf("loading /tmp/cuckoo into newcf\n");
	result = cuckoo_load(&newcf, "/tmp/cuckoo");
	if (result != true) {
		fprintf(stderr, "failed to load /tmp/cuckoo\n");
		return EXIT_FAILURE;
	}
	cuckoo_save(newcf, "/tmp/cuckoo_newcf");

	result = cuckoo_lookup_string(newcf, "beep");
	printf("beep: %d\n", result);
	if (result != true) {
		fprintf(stderr, "FATAL: \"beep\" should be in filter\n");
		return EXIT_FAILURE;
	}

	result = cuckoo_lookup(newcf, "boop", strlen("boop"));
	printf("boop: %d\n", result);
	if (result != true) {
		fprintf(stderr, "FATAL: \"boop\" should be in filter\n");
		return EXIT_FAILURE;
	}

	result = cuckoo_lookup_string(newcf, "doot");
	printf("doot: %d\n", result);
	if (result != false) {
		fprintf(stderr, "FATAL: \"doot\" should be in filter\n");
		return EXIT_FAILURE;
	}

	remove("/tmp/cuckoo");
	remove("/tmp/cuckoo_newcf");

	cuckoo_destroy(&newcf);
	cuckoo_destroy(&cf);

	return EXIT_SUCCESS;
}
