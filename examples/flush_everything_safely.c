#include <stdio.h>
#include <stdlib.h>
#include "../libhslice.h"

void check_error(int val) {
	if (val < 0) {
		fprintf(stderr, "Error occured\n");
		exit(EXIT_FAILURE);
	}
}

int main(int argc, char **argv) {
	hslice_obj a;

	if (argc > 3) a = hslice_open(argv[1]); else return EXIT_FAILURE;
	int count = hslice_count(&a);
	check_error(count);
	hslice_parse(&a, argv[2], argv[3]);
	check_error(count);
	if (count == 0) {
		fprintf(stderr, "No data recieved\n");
		hslice_close(&a);
		return EXIT_SUCCESS;
	}
	if (count < 0) {
		fprintf(stderr, "Error occured\n");
		return EXIT_FAILURE;
	}
	size_t i = 0;
	while (i < count) {
		printf("%s %s\n", hslice_tags(&a)[i], hslice_return(&a, hslice_tags(&a)[i]));
		i++;
	}
	hslice_close(&a);
	return EXIT_SUCCESS;
}
