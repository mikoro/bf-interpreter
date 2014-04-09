# Brainfuck Interpreter

This is a complete interpreter for the [Brainfuck language](http://en.wikipedia.org/wiki/Brainfuck).

Notable features:

- Can read input from both standard input and from files.
- Syntax checking.
- Data segment size is configurable.
- Bounds checking for the data segment and data cells.
- Prints row and column info in errors.

## Example

Here's a basic hello world program written in this language:

    ++++++++++[>+++++++>++++++++++>+++>+<<<<-]>++.>+.+++++++..+++.>++.<<++++++++[++++++[+.>.+++.--[----.----[----.>+.>.

It should output "Hello World!".

## Compiling

The program should compile on any recent linux machine with the included makefile.
