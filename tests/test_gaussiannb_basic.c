#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "gaussiannb.h"

int main() {
	gaussiannb gnb;

	bool result;

	// 3 classes, 2 features
	result = gaussiannb_init(&gnb, 3, 2);
	if (result != true) {
		fprintf(stderr, "FATAL: gaussiannb_init()\n");
		return EXIT_FAILURE;
	}

	double *X[] = {
		(double[]) { 1.0, 2.0 },
		(double[]) { 2.0, 3.0 },

		(double[]) { 3.0, 4.0 },
		(double[]) { 4.0, 5.0 },

		(double[]) { 5.0, 6.0 },
		(double[]) { 6.0, 7.0 },
	};
	int y[] = { 0, 0, 1, 1, 2, 2 };

	gaussiannb_train(&gnb, X, y, sizeof(X) / sizeof(X[0]));

	double class0[] = { 2.5, 3.5 };
	double class1[] = { 4.0, 4,0 };
	double class2[] = { 6.0, 6.5 };

	int prediction = gaussiannb_predict(&gnb, class0);
	printf("predicted class0: %d\n", prediction);
	if (prediction != 0) {
		fprintf(stderr, "FAILURE: prediction should be 0\n");
		return EXIT_FAILURE;
	}

	prediction = gaussiannb_predict(&gnb, class1);
	printf("predicted class1: %d\n", prediction);
	if (prediction != 1) {
		fprintf(stderr, "FAILURE: prediction should be 1\n");
		return EXIT_FAILURE;
	}

	prediction = gaussiannb_predict(&gnb, class2);
	printf("predicted class2: %d\n", prediction);
	if (prediction != 2) {
		fprintf(stderr, "FAILURE: prediction should be 2\n");
		return EXIT_FAILURE;
	}

	printf("distance from class0 to class2 sample: %f\n",
		   gaussiannb_mahalanobis_distance(&gnb, class2, 0));

	printf("distance from class1 to class2 sample: %f\n",
		   gaussiannb_mahalanobis_distance(&gnb, class2, 1));

	printf("distance from class2 to class2 sample: %f\n",
		   gaussiannb_mahalanobis_distance(&gnb, class2, 2));


	gaussiannb_destroy(&gnb);

	return EXIT_SUCCESS;
}
