#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include "mmh3.h"

// TODO test 64 and 128 bit hashing
// TODO test for collision rate
// TODO test avalanche effect
// TODO test entropy
// TODO test correlation; x should be vastly different than x+1

static void random_string(char *str, size_t length) {
    const char alphabet[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    size_t charset_size = sizeof(alphabet) - 1;

    for (size_t i = 0; i < length; i++) {
        int random_index = rand() % charset_size;
        str[i] = alphabet[random_index];
    }

    str[length] = '\0';
}

double mean(int buckets[], size_t total) {
	double sum = 0.0;

	for (size_t i = 0; i < total; i++) {
		sum += buckets[i];
	}

	return sum / total;
}

static double variance(int buckets[], int total) {
	double m = mean(buckets, total);
	double variance = 0.0;

	for (size_t i = 0; i < total; i++) {
		variance += pow(buckets[i] - m, 2);
	}

	return variance / total;
}

void test_uniform_distribution_mmh3_32(size_t num_buckets, size_t iterations) {
	int bucket[num_buckets];

	for (size_t i = 0; i < num_buckets; i++) {
		bucket[i] = 0;
	}

	for (size_t i = 0; i < iterations; i++) {
		char buf[32];
		random_string(buf, sizeof(buf) - 1);
		uint32_t result = mmh3_32_string(buf, 0);
		bucket[result % num_buckets]++;
	}

	int expected = iterations / num_buckets;
	double v = variance(bucket, num_buckets);
	double acceptable = 0.1;
	printf("%f -- %d -- %s\n",
		   v,
		   expected,
		   ((float)abs(v - expected) / (float)expected) < acceptable ? "acceptable" : "unacceptable");
}

int main() {
	srand(time(NULL));
	test_uniform_distribution_mmh3_32(1000, 100000);
	return EXIT_SUCCESS;
}
