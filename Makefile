all:
	cc main.c -o hslice_tests --std=c99 -O2 -Wall -Werror -Wextra
clean:
	rm hslice_tests
dev:
	cc main.c -o hslice_tests --std=c99 -Wall -Wextra -O0 -g
