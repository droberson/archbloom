#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

//#include <archbloom/bloom.h>
#include "../src/bloom.h"

size_t         verbosity   = 0;

static void strip(char *line) {
	size_t len = strlen(line);
	while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
		line[--len] = '\0';
	}
}

static int create(const char *outfile,
				  const char *input_file,
				  const char *name,
				  uint64_t expected_elements,
				  float accuracy) {
	bloomfilter bf;
	FILE *fp = stdin;
	if (input_file != NULL) {
		fp = fopen(input_file, "r");
		if (!fp) {
			fprintf(stderr,
					"unable to open input file %s:%s \n",
					input_file,
					strerror(errno));
			return EXIT_FAILURE;
		}
	}

	// initialize filter
	bloom_error_t bf_err = bloom_init(&bf, expected_elements, accuracy);
	if (bf_err != BF_SUCCESS) {
		fprintf(stderr,
				"error initializing filter: %s\n",
				bloom_strerror(bf_err));
		return EXIT_FAILURE;
	}

	// read from input file or stdin
	char buf[1024]; // TODO define this
	while (fgets(buf, sizeof(buf), fp)) {
		strip(buf);
		bloom_add_string(&bf, buf);
	}

	// set name of filter
	if (name != NULL) {
		bloom_set_name(&bf, name);
	}

	// save filter
	bf_err = bloom_save(&bf, outfile);
	if (bf_err != BF_SUCCESS) {
		fprintf(stderr,
				"unable to save filter to %s: %s\n",
				outfile,
				bloom_strerror(bf_err));
		return EXIT_FAILURE;
	}

	// TODO report of number of elements added to filter and saturation

	// cleanup
	if (fp != stdin) {
		fclose(fp);
	}

	bloom_destroy(&bf);

	return EXIT_SUCCESS;
}

static int query(const char *query_file, const char *query_string) {
	bloomfilter bf;
	bloom_error_t bf_err = bloom_load(&bf, query_file);

	if (bf_err != BF_SUCCESS) {
		fprintf(stderr,
				"unable to open filter %s: %s\n",
				query_file,
				bloom_strerror(bf_err));
		return EXIT_FAILURE;
	}

	bool result = bloom_lookup_string(&bf, query_string);

	if (verbosity > 0) {
		fprintf(stderr,
				"%s %s in filter %s\n",
				query_string,
				result ? "is" : "is NOT",
				query_file);
	}

	bloom_destroy(&bf);
	return result ? EXIT_SUCCESS : EXIT_FAILURE;
}

static int rename_filter(const char *rename_file, const char *new_name) {
	bloomfilter bf;
	bloom_error_t bf_err = bloom_load(&bf, rename_file);
	if (bf_err != BF_SUCCESS) {
		fprintf(stderr,
				"unable to open filter %s: %s\n",
				rename_file,
				bloom_strerror(bf_err));
		return EXIT_FAILURE;
	}

	bloom_set_name(&bf, new_name);
	bf_err = bloom_save(&bf, rename_file);
	if (bf_err != BF_SUCCESS) {
		fprintf(stderr,
				"unable to save filter %s: %s\n",
				rename_file,
				bloom_strerror(bf_err));
		return EXIT_FAILURE;
	}

	bloom_destroy(&bf);
	return EXIT_SUCCESS;

}

static int info(const char *path) {
	bloomfilter bf;
	bloom_error_t bf_err = bloom_load(&bf, path);
	if (bf_err != BF_SUCCESS) {
		fprintf(stderr,
				"unable to open filter %s: %s\n",
				path,
				bloom_strerror(bf_err));
		return EXIT_FAILURE;
	}

	printf("name:                          %s\n", bf.name);
	printf("filter size (in bits):         %d\n", bf.size);
	printf("hash count per element:        %d\n", bf.hashcount);
	printf("bitmap size (in bytes):        %d\n", bf.bitmap_size);
	printf("expected number of elements:   %d\n", bf.expected);
	printf("desired margin of error:       %f%%\n", bf.accuracy);
	printf("estimated false positive rate: %f%%\n",
		   bloom_estimate_false_positive_rate(&bf));
	printf("saturation:                    %f%%\n", bloom_saturation(&bf));

	bloom_destroy(&bf);
	return EXIT_SUCCESS;
}

static int add(const char *filter_file, const char *element, const char *infile) {
	bloomfilter bf;
	bloom_error_t bf_err = bloom_load(&bf, filter_file);
	if (bf_err != BF_SUCCESS) {
		fprintf(stderr,
				"unable to open filter %s: %s\n",
				filter_file,
				bloom_strerror(bf_err));
		return EXIT_FAILURE;
	}

	if (element == NULL && infile == NULL) { // stdin
		char buf[1024];
		while(fgets(buf, sizeof(buf), stdin)) {
			strip(buf);
			bloom_add_string(&bf, buf);
		}

		bloom_save(&bf, filter_file);
		bloom_destroy(&bf);
		return EXIT_SUCCESS;
	}

	if (infile != NULL) { // add elements from file
		printf("adding elements from %s\n", infile);
		char buf[1024];
		FILE *fp = fopen(infile, "r");
		if (!fp) {
			fprintf(stderr,
					"unable to open file %s: %s\n",
					infile,
					strerror(errno));
			return EXIT_FAILURE;
		}

		while(fgets(buf, sizeof(buf), fp)) {
			strip(buf);
			bloom_add_string(&bf, buf);
		}

		bloom_save(&bf, filter_file);
		bloom_destroy(&bf);
		return EXIT_SUCCESS;
	}

	// add single element to filter
	bloom_add_string(&bf, element);
	bloom_save(&bf, filter_file);
	bloom_destroy(&bf);
	return EXIT_SUCCESS;
}

static int merge(const char *infile1, const char *infile2, const char *outfile) {
	bloomfilter in1, in2, out;
	bloom_error_t bf_err = bloom_load(&in1, infile1);
	if (bf_err != BF_SUCCESS) {
		fprintf(stderr,
				"unable to load input file %s: %s\n",
				infile1,
				bloom_strerror(errno));
		return EXIT_FAILURE;
	}

	bf_err = bloom_load(&in2, infile2);
	if (bf_err != BF_SUCCESS) {
		fprintf(stderr,
				"unable to load input file %s: %s\n",
				infile2,
				bloom_strerror(errno));
		bloom_destroy(&in1);
		return EXIT_FAILURE;
	}

	bf_err = bloom_init(&out, in1.expected, in1.accuracy);
	if (bf_err != BF_SUCCESS) {
		fprintf(stderr,
				"unable to create output filter: %s\n",
				bloom_strerror(errno));
		bloom_destroy(&in1);
		bloom_destroy(&in2);
		return EXIT_FAILURE;
	}

	bf_err = bloom_merge(&out, &in1, &in2);
	if (bf_err != BF_SUCCESS) {
		fprintf(stderr,
				"unable to merge bloom filters %s and %s: %s\n",
				infile1,
				infile2,
				bloom_strerror(bf_err));
		bloom_destroy(&in1);
		bloom_destroy(&in2);
		bloom_destroy(&out);
		return EXIT_FAILURE;
	}

	bf_err = bloom_save(&out, outfile);
	if (bf_err != BF_SUCCESS) {
		fprintf(stderr,
				"unable to save output file %s: %s\n",
				outfile,
				bloom_strerror(errno));
		bloom_destroy(&in1);
		bloom_destroy(&in2);
		bloom_destroy(&out);
		return EXIT_FAILURE;
	}

	bloom_destroy(&in1);
	bloom_destroy(&in2);
	bloom_destroy(&out);

	return EXIT_SUCCESS;
}

static int intersect(const char *infile1, const char *infile2, const char *outfile) {
	bloomfilter in1, in2, out;
	bloom_error_t bf_err = bloom_load(&in1, infile1);
	if (bf_err != BF_SUCCESS) {
		fprintf(stderr,
				"unable to load input file %s: %s\n",
				infile1,
				bloom_strerror(errno));
		return EXIT_FAILURE;
	}

	bf_err = bloom_load(&in2, infile2);
	if (bf_err != BF_SUCCESS) {
		fprintf(stderr,
				"unable to load input file %s: %s\n",
				infile2,
				bloom_strerror(errno));
		bloom_destroy(&in1);
		return EXIT_FAILURE;
	}

	bf_err = bloom_init(&out, in1.expected, in1.accuracy);
	if (bf_err != BF_SUCCESS) {
		fprintf(stderr,
				"unable to create output filter: %s\n",
				bloom_strerror(errno));
		bloom_destroy(&in1);
		bloom_destroy(&in2);
		return EXIT_FAILURE;
	}

	bf_err = bloom_intersect(&out, &in1, &in2);
	if (bf_err != BF_SUCCESS) {
		fprintf(stderr,
				"unable to intersect bloom filters %s and %s: %s\n",
				infile1,
				infile2,
				bloom_strerror(bf_err));
		bloom_destroy(&in1);
		bloom_destroy(&in2);
		bloom_destroy(&out);
		return EXIT_FAILURE;
	}

	bf_err = bloom_save(&out, outfile);
	if (bf_err != BF_SUCCESS) {
		fprintf(stderr,
				"unable to save output file %s: %s\n",
				outfile,
				bloom_strerror(errno));
		bloom_destroy(&in1);
		bloom_destroy(&in2);
		bloom_destroy(&out);
		return EXIT_FAILURE;
	}

	bloom_destroy(&in1);
	bloom_destroy(&in2);
	bloom_destroy(&out);

	return EXIT_SUCCESS;
}

static float intersection(const char *infile1, const char *infile2) {
	bloomfilter in1, in2;

	bloom_error_t bf_err = bloom_load(&in1, infile1);
	if (bf_err != BF_SUCCESS) {
		fprintf(stderr,
				"unable to load input filter %s: %s\n",
				infile1,
				bloom_strerror(bf_err));
		return -1.0f;
	}

	bf_err = bloom_load(&in2, infile2);
	if (bf_err != BF_SUCCESS) {
		fprintf(stderr,
				"unable to load input filter %s: %s\n",
				infile2,
				bloom_strerror(bf_err));
		return -1.0f;
	}

	return bloom_estimate_intersection(&in1, &in2);
}

// TODO this is wrong. fix the output once usage is settled
static void usage(const char *progname) {
	fprintf(stderr, "usage: %s COMMAND [OPTIONS]\n\n", progname);
	fprintf(stderr, "Commands: query|lookup, info\n");
	exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
	int opt;

	if (argc < 2) {
		usage(argv[0]);
	}

	const char *command = argv[1];

	// info command
	if (strcmp(command, "info") == 0) {
		if (argc < 3) {
			fprintf(stderr, "must provide a file to display info of\n");
			fprintf(stderr,	"ex: %s info /path/to/filter\n", argv[0]);
			return EXIT_FAILURE;
		}
		const char *info_file = argv[2];

		return info(info_file);
	}

	// create command
	// bloomtool create outfile expected [-i infile -n name -a accuracy]
	else if (strcmp(command, "create") == 0) {
		if (argc < 3) {
			fprintf(stderr, "must provide a path to filter output file\n");
			fprintf(stderr,
					"ex: %s create file 1000 [-n name -i infile -a accuracy]\n",
					argv[0]);
			return EXIT_FAILURE;
		}
		const char *outfile = argv[2];

		uint64_t expected_elements;
		if (argc < 4) {
			fprintf(stderr, "must provide expected number of elements\n");
			fprintf(stderr,
					"ex: %s create file 1000 [-n name -i infile -a accuracy]\n",
					argv[0]);
			return EXIT_FAILURE;
		}
		expected_elements = atoll(argv[3]);

		float accuracy = 0.01; // default is 99.9% accuracy
		char *input_file = NULL;
		char *name = NULL;
		while((opt = getopt(argc - 2, argv + 2, "n:i:a:")) != -1) {
			switch (opt) {
			case 'a':
				accuracy = atof(optarg);
				break;
			case 'i':
				input_file = optarg;
				break;
			case 'n':
				name = optarg;
				break;
			default:
				fprintf(stderr,
						"usage: %s create 1000 [-n filtername -i inputfile -a accuracy]\n",
						argv[0]);
				return EXIT_FAILURE;
			}
		}

		return create(outfile, input_file, name, expected_elements, accuracy);
	}

	// add command -- from file or stdin or a single element
	// bloomtool add filterfile [-i infile] [element]
	// if no infile or element, read from stdin
	// TODO add if not in filter; lookup returns true if in filter, false if not
	//      and adds if not in filter
	else if (strcmp(command, "add") == 0) {
		if (argc < 3) {
			fprintf(stderr, "must provide a filter file to add elements to\n");
			fprintf(stderr,
					"ex: %s add foo.filter \"your cool element here\"\n",
					argv[0]);
			return EXIT_FAILURE;
		}
		const char *filter_file = argv[2];

		char *infile = NULL;
		while((opt = getopt(argc - 2, argv + 2, "i:")) != -1) {
			switch (opt) {
			case 'i':
				printf("setting infile to %s\n", optarg);
				infile = optarg;
				break;
			default:
				fprintf(stderr,
						"ex: %s add file [string] [-i infile]\n"
						"if no string is provided, assume stdin\n",
						argv[0],
						command);
				return EXIT_FAILURE;
			}
		}

		argv += optind;
		argc -= optind;

		if (argv[2] == NULL && !infile) { // assume stdin
			return add(filter_file, NULL, NULL);
		}

		if (infile != NULL) { // input file
			return add(filter_file, NULL, infile);
		}

		// add single element
		const char *element = argv[2];
		return add(filter_file, element, infile);
	}

	// rename command
	else if (strcmp(command, "rename") == 0) {
		if (argc < 3) {
			fprintf(stderr, "must provide a path to a filter to rename\n");
			fprintf(stderr,
					"ex: %s rename /path/to/filter new_name\n",
					argv[0]);
			return EXIT_FAILURE;
		}
		const char *rename_file = argv[2];

		if (argc < 4) {
			fprintf(stderr, "must provide a new name for the filter\n");
			fprintf(stderr,
					"ex: %s rename /path/to/filter new_name\n",
					argv[0]);
			return EXIT_FAILURE;
		}
		const char *new_name = argv[3];

		return rename_filter(rename_file, new_name);
	}

	// merge command
	// TODO option to set name of output merged filter
	else if (strcmp(command, "merge") == 0) {
		if (argc < 5) {
			fprintf(stderr, "must provide two identically sized filters and an outfile\n");
			fprintf(stderr, "ex: %s merge infile1 infile2 outfile\n");
			return EXIT_FAILURE;
		}

		const char *in1 = argv[2];
		const char *in2 = argv[3];
		const char *out = argv[4];

		return merge(in1, in2, out);
	}

	// intersect command
	// TODO option to set name of output intersected filter
	else if (strcmp(command, "intersect") == 0) {
		if (argc < 5) {
			fprintf(stderr, "must provide two identically sized filters and an outfile\n");
			fprintf(stderr, "ex: %s intersect infile1 infile2 outfile\n");
			return EXIT_FAILURE;
		}

		const char *in1 = argv[2];
		const char *in2 = argv[3];
		const char *out = argv[4];
		return intersect(in1, in2, out);
	}

	// intersection comand
	else if (strcmp(command, "intersection") == 0) {
		if (argc < 4) {
			fprintf(stderr, "must provide two identically-sized filters\n");
			return EXIT_FAILURE;
		}

		const char *in1 = argv[2];
		const char *in2 = argv[3];

		float result = intersection(in1, in2);
		if (result == -1.0f) {
			fprintf(stderr,
					"incompatible filters: %s and %s\n",
					in1,
					in2);
			return EXIT_FAILURE;
		}

		printf("intersection of %s and %s: %f%%\n", in1, in2, result);
		return EXIT_SUCCESS;
	}

	// query|lookup command
	else if (strcmp(command, "lookup") == 0 || strcmp(command, "query") == 0) {
		if (argc < 3) {
			fprintf(stderr, "must provide a file to query\n");
			fprintf(stderr,
					"ex: %s %s /path/to/filter string\n",
					argv[0],
					command);
			return EXIT_FAILURE;
		}
		const char *query_file = argv[2];

		if (argc < 4) {
			fprintf(stderr, "must provide a query string\n");
			fprintf(stderr,
					"ex: %s %s /path/to/filter string\n",
					argv[0],
					command);
			return EXIT_FAILURE;
		}
		const char *query_string = argv[3];

		while((opt = getopt(argc - 2, argv + 2, "v")) != -1) {
			switch (opt) {
			case 'v':
				verbosity++;
				break;
			default:
				fprintf(stderr,
						"example usage: %s %s file string [-v]\n",
						argv[0],
						command);
				return EXIT_FAILURE;
			}
		}

		return query(query_file, query_string);
	}

	// if we got here, something went wrong.
	usage(argv[0]);
	return EXIT_FAILURE;
}
