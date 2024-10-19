/* gaussiannb.c
 */
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>

#include "gaussiannb.h"

bool gaussiannb_init(gaussiannb *gnb, size_t num_classes, size_t num_features) {
	gnb->num_classes  = num_classes;
	gnb->num_features = num_features;
	gnb->classes      = calloc(num_classes, sizeof(gaussiannbclass));

	if (gnb->classes == NULL) {
		return false;
	}

	return true;
}

void gaussiannb_destroy(gaussiannb gnb) {
	if (gnb.num_classes > 0) {
		free(gnb.classes[0].mean);
		free(gnb.classes[0].variance);
	}

	free(gnb.classes);
}

void gaussiannb_train(gaussiannb *gnb, double **X, int *y, size_t num_samples) {
	size_t  bufsiz    = gnb->num_classes * gnb->num_features;
	double *means     = calloc(bufsiz, sizeof(double));
	double *variances = calloc(bufsiz, sizeof(double));

	if (means == NULL || variances == NULL) {
		free(means);
		free(variances);
		return;
	}

	for (size_t c = 0; c < gnb->num_classes; c++) {
		gnb->classes[c].mean = means + (c * gnb->num_features);
		gnb->classes[c].variance = variances + (c * gnb->num_features);
		size_t count = 0;

		// calculate mean and variance of features
		for (size_t i = 0; i < num_samples; i++) {
			if (y[i] == c) {
				count++;
				for (size_t j = 0; j < gnb->num_features; j++) {
					gnb->classes[c].mean[j] += X[i][j];
				}
			}
		}

		for (size_t j = 0; j < gnb->num_features; j++) {
			gnb->classes[c].mean[j] /= count;
		}

		for (size_t i = 0; i < num_samples; i++) {
			if (y[i] == c) {
				for (size_t j = 0; j < gnb->num_features; j++) {
					gnb->classes[c].variance[j] += pow(X[i][j] - gnb->classes[c].mean[j], 2);
				}
			}
		}

		for (size_t j = 0; j < gnb->num_features; j++) {
			gnb->classes[c].variance[j] /= count;
		}

		gnb->classes[c].prior = (double)count / num_samples;
	}
}

int gaussiannb_predict(gaussiannb *gnb, double *X) {
	double best_posterior = -INFINITY;
	int    best_class     = -1;
	double epsilon        = 1e-9; // avoid division by zero

	for (size_t c = 0; c < gnb->num_classes; c++) {
		double log_prob = log(gnb->classes[c].prior);

		// calculate log(probabilities) of features
		for (size_t j = 0; j < gnb->num_features; j++) {
			double mean = gnb->classes[c].mean[j] + epsilon;
			double var  = gnb->classes[c].variance[j];
			double prob = (1 / sqrt(2 * M_PI * var)) * exp(-pow(X[j] - mean, 2) / (2 * var));
			log_prob += log(prob);
		}

		if (log_prob > best_posterior) {
			best_posterior = log_prob;
			best_class = c;
		}
	}

	return best_class;
}

double gaussiannb_mahalanobis_distance(gaussiannb *gnb, double *X, size_t class_index) {
	double distance = 0.0;

	for (size_t i = 0; i < gnb->num_features; i++) {
		double diff = X[i] - gnb->classes[class_index].mean[i];
		distance += (diff * diff) / gnb->classes[class_index].variance[i];
	}

	return sqrt(distance);
}
