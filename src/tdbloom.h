/* tdbloom.h
 */
#ifndef TDBLOOM_H
#define TDBLOOM_H

#include <time.h>
#include <stdint.h>
#include <stdbool.h>

/* tdbloom_error_t -- error handling return values
 */
typedef enum {
	TDBF_SUCCESS,
	TDBF_INVALIDTIMEOUT,
	TDBF_OUTOFMEMORY,
	TDBF_FOPEN,
	TDBF_FREAD,
	TDBF_FWRITE,
	TDBF_FSTAT,
	TDBF_INVALIDFILE,
	// used for counting number of statuses. don't add statuses below this line
	TDBF_ERRORCOUNT
} tdbloom_error_t;

/* tdbloom_errors -- human-readable error messages
 */
const char *tdbloom_errors[] = {
	"Success",
	"Invalid timeout value",
	"Out of memory",
	"Unable to open file",
	"Unable to read file",
	"Unable to write to file",
	"fstat() error",
	"Invalid file format"
};

/* tdbloom -- time-decaying bloom filter structure
 */
typedef struct {
	size_t  size;          /* size of time filter */
	size_t  hashcount;     /* number of hashes per element */
	size_t  timeout;       /* number of seconds an element is valid */
	size_t  filter_size;   /* number of time_t values in time filter */
	time_t  start_time;    /* time of filter initialization */
	size_t  expected;      /* expected number of elements */
	float   accuracy;      /* desired margin of error */
	size_t  max_time;      /* maximum value of timestamp */
	int     bytes;         /* byte size of timestamps */
	void   *filter;        /* array of time_t elements */
} tdbloom;

/* function definitions
 */
tdbloom_error_t  tdbloom_init(tdbloom *,
							  const size_t,
							  const float,
							  const size_t);
void             tdbloom_destroy(tdbloom *);
void             tdbloom_clear(tdbloom *);
size_t           tdbloom_clear_expired(tdbloom *);
size_t           tdbloom_count_expired(tdbloom);
void             tdbloom_reset_start_time(tdbloom *);
float            tdbloom_saturation(const tdbloom);
void             tdbloom_add(tdbloom *, void *, const size_t);
void             tdbloom_add_string(tdbloom, const char *);
bool             tdbloom_lookup(const tdbloom, void *, const size_t);
bool             tdbloom_lookup_string(const tdbloom, const char *);
tdbloom_error_t  tdbloom_save(tdbloom, const char *);
tdbloom_error_t  tdbloom_load(tdbloom *, const char *);
const char      *tdbloom_strerror(tdbloom_error_t);

#endif /* TDBLOOM_H */
