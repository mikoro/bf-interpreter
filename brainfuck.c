// Copyright 2007-2013 Mikko Ronkainen <firstname@mikkoronkainen.com>
// Licensed under the Apache License, Version 2.0 (see the LICENSE file)

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define VERSION "1.0.0"
#define DEFAULT_DATA_SIZE 30000

typedef struct
{
	char* filePath;				// file path for the code segment
	uint8_t* code;				// code pointer and code segment
	uint8_t* codeOrig;			// code pointer original position
	int8_t* data;				// data pointer and data segment
	int8_t* dataOrig;			// data pointer original position
	uint32_t dataSize;			// data segment size in bytes
	bool enableBoundsCheck;		// enable bounds checking for the data segment
	bool enableWrapCheck;		// enable wrap checking for the data cells
	bool enableSyntaxCheck;		// enable strict syntax checking
	bool enableQuietMode;		// enable quiet mode
	bool showHelp;				// show help at startup
} InterpreterState;

enum errorCodes { E_FILE, E_MEMORY, E_INDEX_ABOVE, E_INDEX_BELOW, E_WRAP_OVER, E_WRAP_UNDER, E_OPEN_BRACKET, E_CLOSE_BRACKET, E_SYNTAX, E_NONE };

const char* errorMessages[] =
{
	"Reading file failed",
	"Memory allocation failed",
	"Indexed above the data segment",
	"Indexed below the data segment",
	"Data cell value wrapped over",
	"Data cell value wrapped under",
	"No match for opening bracket found",
	"No match for closing bracket found",
	"Syntax error",
	"No error"
};

const char* usageText = "Usage: bf [-f <file> | -d <size> | -b | -w | -s | -q | -h]\n(type 'bf -h' for help)\n";
const char* helpText = 	"Brainfuck Interpreter v" VERSION "\n\n"
						"Usage: bf [-f <file> | -d <size> | -b | -w | -s | -q | -h]\n\n"
						"  -f <file>    read code segment from file (default is stdin)\n"
						"  -d <size>    specify data segment size (default is 30000)\n"
						"  -b           enable bounds checking for the data segment\n"
						"  -w           enable under/over wrap checking for the data cells\n"
						"  -s           enable strict syntax checking\n"
						"  -q           enable quiet mode\n"
						"  -h           show this help text\n\n";

// allowed characters for the syntax checking (in addition to original bf instructions)
const char* allowedCharacters = "\n";

bool initializeState(int argc, char* argv[], InterpreterState* state);
void uninitializeState(InterpreterState* state);
int readCodeFromFile(InterpreterState* state);
int readCodeFromStdin(InterpreterState* state);
int interpretCode(InterpreterState* state);
bool matchBracket(InterpreterState* state, bool forward);
void findPosition(InterpreterState* state, int* row, int* column);

int main(int argc, char* argv[])
{
	InterpreterState state;

	if(!initializeState(argc, argv, &state))
	{
		printf(usageText);
		return EXIT_FAILURE;
	}
	else if(state.showHelp)
	{
		printf(helpText);
		return EXIT_SUCCESS;
	}

	int error;

	// read code segment either from file or stdin
	if(state.filePath)
		error = readCodeFromFile(&state);
	else
	{
		if(!state.enableQuietMode)
			printf("Type in the code (issue ^D to stop):\n");

		error = readCodeFromStdin(&state);

		if(!state.enableQuietMode && error == E_NONE)
			printf("Running the program...\n");
	}

	// check if code segment input succeeded
	if(error != E_NONE)
	{
		if(!state.enableQuietMode)
			printf("Error: %s\n", errorMessages[error]);

		return EXIT_FAILURE;
	}
	else if(!state.code)
	{
		if(!state.enableQuietMode)
			printf("No code to be interpreted!\n");

		return EXIT_FAILURE;
	}

	// allocate memory for the data segment
	if(!(state.data = state.dataOrig = (int8_t*)calloc(state.dataSize, sizeof(int8_t))))
	{
		if(!state.enableQuietMode)
			printf("Error: %s\n", errorMessages[E_MEMORY]);

		uninitializeState(&state);
		return EXIT_FAILURE;
	}

	error = interpretCode(&state);

	if(error != E_NONE)
	{
		if(!state.enableQuietMode)
		{
			int row, column;
			findPosition(&state, &row, &column);
			printf("Error: %s at %d:%d (code: '%c' data: '%d')\n", errorMessages[error], row, column, *state.code, *state.data);
		}

		uninitializeState(&state);
		return EXIT_FAILURE;
	}

	uninitializeState(&state);
	return EXIT_SUCCESS;
}

// initialize interpreter state from the command line parameters
// return true if successfull, false otherwise
bool initializeState(int argc, char* argv[], InterpreterState* state)
{
	state->filePath = NULL;
	state->code = NULL;
	state->codeOrig = NULL;
	state->data = NULL;
	state->dataOrig = NULL;
	state->dataSize = DEFAULT_DATA_SIZE;
	state->enableBoundsCheck = false;
	state->enableWrapCheck = false;
	state->enableSyntaxCheck = false;
	state->enableQuietMode = false;
	state->showHelp = false;

	bool valid = true;

	for(int i=1; i<argc && valid; ++i)
	{
		// only arguments like "-x" are considered valid
		if((strlen(argv[i]) == 2) && (argv[i][0] == '-'))
		{
			switch(argv[i][1])
			{
				// external file
				case 'f':
				{
					if(++i < argc)
						state->filePath = argv[i];
					else
						valid = false;
				} break;

				// explicit data size
				case 'd':
				{
					if(++i < argc)
					{
						if((state->dataSize = atoi(argv[i])) <= 0)
							valid = false;
					}
					else
						valid = false;
				} break;

				// all the other state variables
				case 'b': state->enableBoundsCheck = true; break;
				case 'w': state->enableWrapCheck = true; break;
				case 's': state->enableSyntaxCheck = true; break;
				case 'q': state->enableQuietMode = true; break;
				case 'h': state->showHelp = true; break;

				default: valid = false;
			}
		}
		else
			valid = false;
	}

	return valid;
}

// free all resources
void uninitializeState(InterpreterState* state)
{
	free(state->codeOrig);
	free(state->dataOrig);
}

// initialize code segment from file
// return E_NONE if successfull, any other if failed
int readCodeFromFile(InterpreterState* state)
{
	FILE* file = fopen(state->filePath, "r");

	if(!file)
		return E_FILE;

	// determine the file length
	fseek(file, 0, SEEK_END);
	uint32_t length = ftell(file);
	rewind(file);

	if(length > 0)
	{
		// allocate memory for the code segment
		if(!(state->code = state->codeOrig = (uint8_t*)malloc(sizeof(uint8_t) * length)))
			return E_MEMORY;

		fread(state->code, sizeof(uint8_t), length, file);
		state->code[length - 1] = 0;
		fclose(file);
	}

	return E_NONE;
}

// initialize code segment from stdin
// return E_NONE if successfull, any other if failed
int readCodeFromStdin(InterpreterState* state)
{
	int c;
	uint32_t length = 0;

	while((c = getchar()) != EOF)
	{
		// allocate memory for the code segment, realloc with every new character
		if(!(state->code = state->codeOrig = (uint8_t*)realloc(state->code, sizeof(uint8_t) * ++length)))
			return E_MEMORY;

		state->code[length - 1] = (uint8_t)c;
	}

	if(length > 0)
		state->code[length - 1] = 0;

	return E_NONE;
}

// interpret the code segment
// return E_NONE if successfull, any other if failed
int interpretCode(InterpreterState* state)
{
	// loop until a zero character
	while(*state->code)
	{
		switch(*state->code)
		{
			// move the pointer to the right
			case '>':
			{
				if(state->enableBoundsCheck && ((uint32_t)(state->data - state->dataOrig) == state->dataSize))
					return E_INDEX_ABOVE;

				++state->data;
			} break;

			// move the pointer to the left
			case '<':
			{
				if(state->enableBoundsCheck && (state->data - state->dataOrig == 0))
					return E_INDEX_BELOW;

				--state->data;
			} break;

			// increment the memory cell under the pointer
			case '+':
			{
				if(state->enableWrapCheck && (*state->data == INT8_MAX))
					return E_WRAP_OVER;

				++*state->data;
			} break;

			// decrement the memory cell under the pointer
			case '-':
			{
				if(state->enableWrapCheck && (*state->data == INT8_MIN))
					return E_WRAP_UNDER;

				--*state->data;
			} break;

			// jump past the matching ] if the cell under the pointer is 0
			case '[':
			{
				if(!*state->data && !matchBracket(state, true))
					return E_OPEN_BRACKET;
			} break;

			// jump back to the matching [ if the cell under the pointer is nonzero
			case ']':
			{
				if(*state->data && !matchBracket(state, false))
					return E_CLOSE_BRACKET;
			} break;

			// output the character signified by the cell at the pointer
			case '.': putchar(*state->data); break;
			
			// input a character and store it in the cell at the pointer
			case ',': *state->data = (int8_t)getchar(); break;
			
			default:
			{
				if(state->enableSyntaxCheck && !strchr(allowedCharacters, *state->code))
					return E_SYNTAX;
			}
		}

		++state->code;
	}

	return E_NONE;
}

// move code pointer to matching bracket
// return true if match was found, false otherwise
bool matchBracket(InterpreterState* state, bool forward)
{
	uint8_t* codeCurrent = state->code;

	// loop until matching bracket was found (and take into account possible nested brackets)
	for(int i=0; i += (*state->code == '[') - (*state->code == ']'); state->code += (forward ? 1 : -1))
	{
		// check that we don't under/overflow
		if(state->code - state->codeOrig < 0 || !*state->code)
		{
			state->code = codeCurrent;
			return false;
		}
	}

	return true;
}

// determine row and column position of the current code pointer inside the code segment
void findPosition(InterpreterState* state, int* row, int* column)
{
	int x = 1, y = 1;

	// loop from the beginning of the code segment to current code pointer position
	for(int i=0; i < (state->code - state->codeOrig); ++i)
	{
		++x;

		if(state->codeOrig[i] == '\n')
		{
			x = 1;
			++y;
		}
	}

	*row = y;
	*column = x;
}
