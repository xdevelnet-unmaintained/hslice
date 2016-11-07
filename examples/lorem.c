#include <stdio.h>
#include "../libhslice.h"

int main(int argc, char **argv) {
	hslice_obj a = hslice_open("lorem.txt");;
	hslice_parse(&a, "{Z_", "}");
	printf("string from O tag: %s\nstring from V tag: %s\n", hslice_return(&a, "O"), hslice_return(&a, "V"));
	hslice_close(&a);
	return 0;
}
