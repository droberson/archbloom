#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#include "tdcbloom.h"

void tdcbloom_print_entries(const tdcbloom *tdcbf) {
    size_t entry_size = 0;

    // Calculate entry size based on counter and timer sizes
    entry_size = ((tdcbf->counter_size == COUNTER_8BIT ? sizeof(uint8_t) :
                   tdcbf->counter_size == COUNTER_16BIT ? sizeof(uint16_t) :
                   tdcbf->counter_size == COUNTER_32BIT ? sizeof(uint32_t) :
                   sizeof(uint64_t)) +
                  (tdcbf->timer_size == TIMER_8BIT ? sizeof(uint8_t) :
                   tdcbf->timer_size == TIMER_16BIT ? sizeof(uint16_t) :
                   tdcbf->timer_size == TIMER_32BIT ? sizeof(uint32_t) :
                   sizeof(uint64_t)));

    // Iterate over each entry in the entrymap
    for (size_t i = 0; i < tdcbf->size; i++) {
        // Calculate the base address of the current entry
        uint8_t *entry_base = (uint8_t *)tdcbf->entrymap + (i * entry_size);

        // Access the counter
        void *counter_ptr = entry_base;
        printf("Entry %zu: Counter = ", i);
        switch (tdcbf->counter_size) {
            case COUNTER_8BIT:
                printf("%" PRIu8, *(uint8_t *)counter_ptr);
                break;
            case COUNTER_16BIT:
                printf("%" PRIu16, *(uint16_t *)counter_ptr);
                break;
            case COUNTER_32BIT:
                printf("%" PRIu32, *(uint32_t *)counter_ptr);
                break;
            case COUNTER_64BIT:
                printf("%" PRIu64, *(uint64_t *)counter_ptr);
                break;
        }

        // Access the timestamp
        void *timestamp_ptr = entry_base + (entry_size - (tdcbf->timer_size == TIMER_8BIT ? sizeof(uint8_t) :
                                                          tdcbf->timer_size == TIMER_16BIT ? sizeof(uint16_t) :
                                                          tdcbf->timer_size == TIMER_32BIT ? sizeof(uint32_t) :
                                                          sizeof(uint64_t)));
        printf(", Timestamp = ");
        switch (tdcbf->timer_size) {
            case TIMER_8BIT:
                printf("%" PRIu8, *(uint8_t *)timestamp_ptr);
                break;
            case TIMER_16BIT:
                printf("%" PRIu16, *(uint16_t *)timestamp_ptr);
                break;
            case TIMER_32BIT:
                printf("%" PRIu32, *(uint32_t *)timestamp_ptr);
                break;
            case TIMER_64BIT:
                printf("%" PRIu64, *(uint64_t *)timestamp_ptr);
                break;
        }

        printf("\n");
    }
}

int main() {
	tdcbloom tdcbf;

	// small filter
	tdcbloom_error_t init_result;
	init_result = tdcbloom_init(&tdcbf,
								10,
								0.01,
								10,
								COUNTER_8BIT,
								TIMER_8BIT);
	if (init_result != TDCBF_SUCCESS) {
		fprintf(stderr, "FAILURE: unable to create first time-decaying, counting bloom filter\n");
		return EXIT_FAILURE;
	}
	printf("10 elements, 99.99%% accuracy, 10 second timeout, 8 bit counter, 8 bit timer\n");
	printf("\tsize: %d\n", tdcbf.size);
	printf("\tstart_time: %d\n", tdcbf.start_time);
	printf("\ttimeout: %d\n", tdcbf.timeout);
	printf("\tmax_time: %d\n", tdcbf.max_time);
	printf("\thashcount: %d\n", tdcbf.hashcount);
	printf("\tcounter_size: %d\n", tdcbf.counter_size);
	printf("\ttimer_size: %d\n", tdcbf.timer_size);

	tdcbloom_add_string(&tdcbf, "go home and be a family man");
	tdcbloom_add_string(&tdcbf, "You must defeat Sheng Long to stand a chance");
	tdcbloom_add_string(&tdcbf, "You must defeat Sheng Long to stand a chance");
	tdcbloom_print_entries(&tdcbf);
	bool result = tdcbloom_lookup_string(&tdcbf, "go home and be a family man");
	if (result != true) {
		fprintf(stderr, "FAILURE: \"go home and be a family man\" should be in filter\n");
		return EXIT_FAILURE;
	}

	size_t count = tdcbloom_count_string(&tdcbf, "You must defeat Sheng Long to stand a chance");
	if (count != 2) {
		fprintf(stderr, "FAILURE: count should be 2\n");
		return EXIT_FAILURE;
	}
	count = tdcbloom_count_string(&tdcbf, "go home and be a family man");
	if (count != 1) {
		fprintf(stderr, "FAILURE: count should be 1\n");
		return EXIT_FAILURE;
	}

	tdcbloom_remove_string(&tdcbf, "go home and be a family man");
	count = tdcbloom_count_string(&tdcbf, "go home and be a family man");
	if (count != 0) {
		fprintf(stderr, "FAILURE: count should be 0\n");
		return EXIT_FAILURE;
	}

	tdcbloom_destroy(&tdcbf);

	// different sized filter.
	init_result = tdcbloom_init(&tdcbf,
								10,
								0.01,
								10,
								COUNTER_16BIT,
								TIMER_32BIT);
	if (init_result != TDCBF_SUCCESS) {
		fprintf(stderr, "FAILURE: unable to create second time-decaying, counting bloom filter\n");
		return EXIT_FAILURE;
	}
	printf("10 elements, 99.99%% accuracy, 10 second timeout, 16 bit counter, 32 bit timer\n");
	printf("\tsize: %d\n", tdcbf.size);
	printf("\tstart_time: %d\n", tdcbf.start_time);
	printf("\ttimeout: %d\n", tdcbf.timeout);
	printf("\tmax_time: %zd\n", tdcbf.max_time);
	printf("\thashcount: %d\n", tdcbf.hashcount);
	printf("\tcounter_size: %d\n", tdcbf.counter_size);
	printf("\ttimer_size: %d\n", tdcbf.timer_size);
	tdcbloom_destroy(&tdcbf);

	return EXIT_SUCCESS;
}
