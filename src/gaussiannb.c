/* gaussiannb.c
 * TODO: save/load models
 * TODO: multi-threaded log calculations for gaussiannb_predict() ?
 */
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>

#include "gaussiannb.h"

bool gaussiannb_init(gaussiannb *gnb, size_t num_classes, size_t num_features) {
	gnb->num_classes  = num_classes;
	gnb->num_features = num_features;
	gnb->num_samples  = 0;
	gnb->classes      = calloc(num_classes, sizeof(gaussiannbclass));

	if (gnb->classes == NULL) {
		return false;
	}

	// initialize class weights to 1.0
	for (size_t c = 0; c < num_classes; c++) {
		gnb->classes[c].weight = 1.0;
	}

	return true;
}

void gaussiannb_destroy(gaussiannb *gnb) {
	if (gnb->num_classes > 0) {
		if (gnb->classes[0].mean) {
			free(gnb->classes[0].mean);
			gnb->classes[0].mean = NULL;
		}

		if (gnb->classes[0].variance) {
			free(gnb->classes[0].variance);
			gnb->classes[0].variance = NULL;
		}
	}

	if (gnb->classes) {
		free(gnb->classes);
		gnb->classes = NULL;
	}
}

static void calculate_class_mean(gaussiannbclass *cls, double **X, int *y, size_t num_samples, size_t num_features, int class_label) {
	size_t count = 0;

	// calculate mean and variance of features
	for (size_t si = 0; si < num_samples; si++) {
		if (y[si] == class_label) {
			count++;
			for (size_t fi = 0; fi < num_features; fi++) {
				if (isnan(X[si][fi])) {
					X[si][fi] = cls->mean[fi];
				}
				cls->mean[fi] += X[si][fi];
			}
		}
	}

	if (count > 0) {
		for (size_t fi = 0; fi < num_features; fi++) {
			cls->mean[fi] /= count;
		}
	}

	cls->count = count;
}

static void calculate_class_variance(gaussiannbclass *cls, double **X, int *y, size_t num_samples, size_t num_features, int class_label, size_t count) {
	for (size_t si = 0; si < num_samples; si++) {
		if (y[si] == class_label) {
			for (size_t fi = 0; fi < num_features; fi++) {
				double squared_deviation = (X[si][fi] - cls->mean[fi]) * (X[si][fi] - cls->mean[fi]);
				cls->variance[fi] += squared_deviation;
			}
		}
	}

	// regularize and normalize variance
	for (size_t fi = 0; fi < num_features; fi++) {
		cls->variance[fi] = (count == 0) ? GNB_EPSILON : (cls->variance[fi] / count) + GNB_ALPHA;
	}
}

void gaussiannb_train(gaussiannb *gnb, double **X, int *y, size_t num_samples) {
	if (num_samples == 0) {
		return;
	}

	size_t  bufsiz    = gnb->num_classes * gnb->num_features;
	double *means     = calloc(bufsiz, sizeof(double));
	double *variances = calloc(bufsiz, sizeof(double));

	if (means == NULL || variances == NULL) {
		if (means != NULL) { free(means); }
		if (variances != NULL) { free(variances); }
		return;
	}

	gnb->num_samples += num_samples; // needed for online learning

	for (size_t ci = 0; ci < gnb->num_classes; ci++) {
		gnb->classes[ci].count    = 0;
		gnb->classes[ci].mean     = means + (ci * gnb->num_features);
		gnb->classes[ci].variance = variances + (ci * gnb->num_features);

		calculate_class_mean(&gnb->classes[ci],
							 X,
							 y,
							 num_samples,
							 gnb->num_features,
							 ci);

		calculate_class_variance(&gnb->classes[ci],
								 X,
								 y,
								 num_samples,
								 gnb->num_features,
								 ci,
								 gnb->classes[ci].count);

		// laplace smoothing using class weight
		gnb->classes[ci].prior = (gnb->classes[ci].count + gnb->classes[ci].weight) / (num_samples + gnb->num_classes);
	}
}

int gaussiannb_predict(gaussiannb *gnb, double *X) {
	double best_posterior = -INFINITY;
	int    best_class     = -1;

	for (size_t c = 0; c < gnb->num_classes; c++) {
		double log_prob = log(gnb->classes[c].prior * gnb->classes[c].weight + GNB_EPSILON);

		// calculate log(probabilities) of features
		for (size_t j = 0; j < gnb->num_features; j++) {
			double mean = gnb->classes[c].mean[j];
			double var  = gnb->classes[c].variance[j] + GNB_EPSILON;
			double diff = X[j] - mean;
			double prob = GNB_NORMALIZING_CONSTANT * exp(-(diff * diff) / (2 * var));
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
		distance += (diff * diff) / (gnb->classes[class_index].variance[i] + GNB_EPSILON);
	}

	return sqrt(distance);
}

void gaussiannb_update(gaussiannb *gnb, double *X, int y, bool new) {
	// handle invalid input
	if (X == NULL || gnb == NULL || y >= gnb->num_classes || y < 0) {
		return;
	}

	for (size_t i = 0; i < gnb->num_features; i++) {
		double old_mean = gnb->classes[y].mean[i];
		gnb->classes[y].mean[i] += (X[i] - old_mean) / (gnb->classes[y].count + 1);
		double old_variance = gnb->classes[y].variance[i];
		gnb->classes[y].variance[i] = (gnb->classes[y].count * old_variance + (X[i] - old_mean) * (X[i] - gnb->classes[y].mean[i])) / (gnb->classes[y].count + 1);
	}

	// avoid double counting
	if (new) {
		gnb->num_samples++;
	}

	gnb->classes[y].count++;
	gnb->classes[y].prior = (double)gnb->classes[y].count / gnb->num_samples;
}

void gaussiannb_adjust_weight(gaussiannb *gnb, int ci, double weight) {
	if (ci >= 0 && ci < gnb->num_classes) {
		gnb->classes[ci].weight = weight;
	}
	// class_index out of range ...
}
