all:
	cc -c libhslice.c -o libhslice.o -fPIC --std=c99 -O2 -Wall -Wextra -Werror
	cc -shared -o libhslice.so libhslice.o --std=c99 -O2 -Wall -Wextra -Werror
dev:
	cc main.c -o hslice_tests --std=c99 -Wall -Wextra -O0 -g
