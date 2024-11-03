/* gaussiannb.h
 */
#ifndef GAUSSIANNB_H
#define GAUSSIANNB_H

#include <math.h>
#include <stdbool.h>
#include <stdlib.h>

#define GNB_EPSILON               1e-9 // avoid division by zero
#define GNB_ALPHA                 1e-2 // for regularization
#define GNB_NORMALIZING_CONSTANT (1 / sqrt(2 * M_PI)) // normalizing constant

/* structures
 */
typedef struct {
	double *mean;
	double *variance;
	double  prior;
	double  weight;
	size_t  count;
} gaussiannbclass;

typedef struct {
	size_t           num_classes;
	size_t           num_features;
	size_t           num_samples;
	gaussiannbclass *classes;
} gaussiannb;

/* function definitions
 * TODO: _update, _train, _adjust_weight, should return statuses
 */
bool   gaussiannb_init(gaussiannb *, size_t, size_t);
void   gaussiannb_destroy(gaussiannb *);
void   gaussiannb_train(gaussiannb *, double **, int *, size_t);
void   gaussiannb_update(gaussiannb *, double *, int, bool);
int    gaussiannb_predict(gaussiannb *, double *);
void   gaussiannb_adjust_weight(gaussiannb *, int, double);
double gaussiannb_mahalanobis_distance(gaussiannb *, double *, size_t);

#endif /* GAUSSIANNB_H */
