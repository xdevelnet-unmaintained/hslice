all:
	cc main.c -o hslice_tests --std=c99 -Wall -Werror -Wextra
clean:
	rm hslice_tests
dev:
	cc main.c -o hslice_tests --std=c99 -Wall -Werror -Wextra -O0 -g
