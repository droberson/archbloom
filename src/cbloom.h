/* cbloom.h
 */
#ifndef CBLOOM_H
#define CBLOOM_H

#include <stdint.h>
#include <stdbool.h>

/* cbloom_error_t - error status type. used for mapping function return values
 *                  to error messages.
 */
typedef enum {
	CBF_SUCCESS = 0,
	CBF_OUTOFMEMORY,
	CBF_INVALIDCOUNTERSIZE,
	CBF_FOPEN,
	CBF_FWRITE,
	CBF_FREAD,
	CBF_FSTAT,
	CBF_INVALIDFILE,
	// dummy enum to use as counter. don't add entries after CBF_ERRORCOUNT
	CBF_ERRORCOUNT
} cbloom_error_t;

const char *cbloom_errors[] = {
	"Success",
	"Out of memory",
	"Invalid counter size",
	"Unable to open file",
	"Unable to write to file",
	"Unable to read file",
	"fstat() failure",
	"Invalid file format"
};

/* counter_size - used for setting appropriately-sized counters, which can
 *                result in a reduced memory footprint if smaller counts are
 *                expected.
 */
typedef enum {
	COUNTER_8BIT,
	COUNTER_16BIT,
	COUNTER_32BIT,
	COUNTER_64BIT
} counter_size;

/* cbloomfilter - structure for a counting bloom filter
 */
typedef struct {
	uint64_t      size;              /* size of counting bloom filter */
	uint64_t      hashcount;         /* number of hashes per element */
	uint64_t      countermap_size;   /* size of map */
	counter_size  csize;             /* size of counter: 8, 16, 32, 64 bit */
	void         *countermap;        /* map of counting bloom filter */
} cbloomfilter;

/* function declarations
 */
cbloom_error_t  cbloom_init(cbloomfilter *, const size_t, const float, counter_size);
void            cbloom_destroy(cbloomfilter *);
size_t          cbloom_count(const cbloomfilter, void *, size_t);
size_t          cbloom_count_string(const cbloomfilter, char *);
bool            cbloom_lookup(const cbloomfilter, void *, const size_t);
bool            cbloom_lookup_string(const cbloomfilter, const char *);
void            cbloom_add(cbloomfilter, void *, const size_t);
void            cbloom_add_string(cbloomfilter, const char *);
void            cbloom_remove(cbloomfilter, void *, const size_t);
void            cbloom_remove_string(cbloomfilter, const char *);
void            cbloom_clear(cbloomfilter *);
size_t          cbloom_saturation_count(const cbloomfilter);
float           cbloom_saturation(const cbloomfilter);
cbloom_error_t  cbloom_save(cbloomfilter, const char *);
cbloom_error_t  cbloom_load(cbloomfilter *, const char *);
const char     *cbloom_strerror(cbloom_error_t);

#endif /* CBLOOM_H */
