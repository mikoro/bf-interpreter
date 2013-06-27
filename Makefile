all:
	gcc -std=c99 -Wall -Wextra -Werror -pedantic -O3 brainfuck.c -o bf
	
debug:
	gcc -std=c99 -Wall -Wextra -Werror -pedantic -ggdb -O0 brainfuck.c -o bfd
