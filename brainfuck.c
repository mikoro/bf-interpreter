// Copyright 2007-2013 Mikko Ronkainen <firstname@mikkoronkainen.com>
// Licensed under the Apache License, Version 2.0 (see the LICENSE file)

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define DEFAULT_DATA_SIZE 30000
#define DEFAULT_ENABLE_STATE false

/// Holds all necessary information for one bf session
typedef struct
{
	char* fileName;				///< Name of the file containing bf code
	int8_t* data;				///< Data segment for the program
	int8_t* dataStart;			///< Same as data, but it will not be modified
	uint32_t dataSize;			///< Size of the data segment in bytes
	uint8_t* code;				///< Actual bf code for the program
	uint8_t* codeStart;			///< Same as code, but it will not be modified
	bool enableBoundsCheck;		///< Enable bounds checking for the data segment
	bool enableWrapCheck;		///< Enable wrap checking for data cells
	bool enableSyntaxCheck;		///< Enable strict syntax check (onlybf instructions + allowedCharacters)
	bool enableQuietMode;		///< While enabled, only output from the actual bf program is displayed
	bool showHelp;				///< Will show the help text at the startup
} InterpreterState;

/// All errors used in this program
enum errorCodes { E_FILE, E_MEMORY, E_INDEX_ABOVE, E_INDEX_BELOW, E_WRAP_OVER, E_WRAP_UNDER, E_OPEN_BRACKET, E_CLOSE_BRACKET, E_SYNTAX, E_NONE };

/// Textual presentation of the error codes
const char* errorMessages[] =
{
	"Reading file failed!",
	"Memory allocation failed!",
	"Indexing above the data segment",
	"Indexing below the data segment",
	"Data cell value wraps over",
	"Data cell value wraps under",
	"No match for opening bracket",
	"No match for closing bracket",
	"Unknown command",
	"No error?"
};

/// Allowed characters for the syntax check (in addition to original bf instructions)
const char* allowedCharacters = "\n";

const char* usageText = "Usage: bf [-f <file> | -b | -w | -s | -q | -h]\n(type 'bf -h' for help)\n";
const char* helpText = 	"Brainfuck interpreter v0.1\n"
						"http://mikoro.iki.fi\n\n"
						"Usage: bf [-f <file> | -b | -w | -s | -q | -h]\n\n"
						"  -f <file>    read bf code from file (default is stdin)\n"
						"  -b           enable bounds checking for the data segment\n"
						"  -w           enable wrap checking for data cells\n"
						"  -s           enable strict syntax check\n"
						"  -q           enable quiet mode\n"
						"  -h           this help text\n\n";

bool initState(int argc, char* argv[], InterpreterState* state);
void freeState(InterpreterState* state);
int readFile(InterpreterState* state);
int readStdin(InterpreterState* state);
int interpret(InterpreterState* state);
bool matchBracket(InterpreterState* state, bool forward);
void findPosition(InterpreterState* state, int* row, int* column);

int main(int argc, char* argv[])
{
	InterpreterState state;

	// init state and display usage/help texts if necessary
	if(!initState(argc, argv, &state))
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

	// read bf code either from file or stdin
	if(state.fileName)
		error = readFile(&state);
	else
	{
		if(!state.enableQuietMode)
			printf("Type in the code (issue ^D to stop):\n");

		error = readStdin(&state);

		if(!state.enableQuietMode && error == E_NONE)
			printf("Running the program...\n");
	}

	// check errors for the code inputting
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
	if(!(state.data = state.dataStart = (int8_t*)calloc(state.dataSize, sizeof(int8_t))))
	{
		if(!state.enableQuietMode)
			printf("Error: %s\n", errorMessages[E_MEMORY]);

		freeState(&state);
		return EXIT_FAILURE;
	}

	error = interpret(&state);

	if(error != E_NONE)
	{
		if(!state.enableQuietMode)
		{
			int row, column;
			findPosition(&state, &row, &column);
			printf("Error: %s at %d:%d (code: '%c' data: '%d')\n", errorMessages[error], row, column, *state.code, *state.data);
		}

		freeState(&state);
		return EXIT_FAILURE;
	}

	freeState(&state);
	return EXIT_SUCCESS;
}

/**
 * Initialize state from command line parameters
 * @param argc Argument count
 * @param argv Argument strings
 * @param state State to be initialized
 * @return True if successfull, false otherwise
 */
bool initState(int argc, char* argv[], InterpreterState* state)
{
	state->fileName = NULL;
	state->data = NULL;
	state->dataStart = NULL;
	state->dataSize = DEFAULT_DATA_SIZE;
	state->code = NULL;
	state->codeStart = NULL;
	state->enableBoundsCheck = DEFAULT_ENABLE_STATE;
	state->enableWrapCheck = DEFAULT_ENABLE_STATE;
	state->enableSyntaxCheck = DEFAULT_ENABLE_STATE;
	state->enableQuietMode = DEFAULT_ENABLE_STATE;
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
						state->fileName = argv[i];
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
				case 'b': state->enableBoundsCheck = !DEFAULT_ENABLE_STATE; break;
				case 'w': state->enableWrapCheck = !DEFAULT_ENABLE_STATE; break;
				case 's': state->enableSyntaxCheck = !DEFAULT_ENABLE_STATE; break;
				case 'q': state->enableQuietMode = !DEFAULT_ENABLE_STATE; break;
				case 'h': state->showHelp = true; break;

				default: valid = false;
			}
		}
		else
			valid = false;
	}

	return valid;
}

/**
 * Free all resources binded to a state
 * @param state State to be freed
 */
void freeState(InterpreterState* state)
{
	free(state->codeStart);
	free(state->dataStart);
}

/**
 * Read bf code from file
 * @param state State where code is saved
 * @return E_NONE if successfull, any other if failed
 */
int readFile(InterpreterState* state)
{
	FILE* file = fopen(state->fileName, "r");

	if(!file)
		return E_FILE;

	// find the file length
	fseek(file, 0, SEEK_END);
	uint32_t length = ftell(file);
	rewind(file);

	if(length > 0)
	{
		// allocate memory for the code
		if(!(state->code = state->codeStart = (uint8_t*)malloc(sizeof(uint8_t) * length)))
			return E_MEMORY;

		fread(state->code, sizeof(uint8_t), length, file);
		state->code[length-1] = 0;
		fclose(file);
	}

	return E_NONE;
}

/**
 * Read bf code from standard input
 * @param state State where code is saved
 * @return E_NONE if successfull, any other if failed
 */
int readStdin(InterpreterState* state)
{
	int c;
	uint32_t length = 0;

	while((c = getchar()) != EOF)
	{
		// allocate memory for the code, realloc with every new character
		if(!(state->code = state->codeStart = (uint8_t*)realloc(state->code, sizeof(uint8_t) * ++length)))
			return E_MEMORY;

		state->code[length-1] = (uint8_t)c;
	}

	if(length > 0)
		state->code[length-1] = 0;

	return E_NONE;
}

/**
 * Interpret bf code
 * @param state State to be interpreted
 * @return E_NONE if successfull, any other if failed
 */
int interpret(InterpreterState* state)
{
	// loop until zero character
	while(*state->code)
	{
		switch(*state->code)
		{
			case '>':
			{
				if(state->enableBoundsCheck && ((uint32_t)(state->data - state->dataStart) == state->dataSize))
					return E_INDEX_ABOVE;

				++state->data;
			} break;

			case '<':
			{
				if(state->enableBoundsCheck && (state->data - state->dataStart == 0))
					return E_INDEX_BELOW;

				--state->data;
			} break;

			case '+':
			{
				if(state->enableWrapCheck && (*state->data == INT8_MAX))
					return E_WRAP_OVER;

				++*state->data;
			} break;

			case '-':
			{
				if(state->enableWrapCheck && (*state->data == INT8_MIN))
					return E_WRAP_UNDER;

				--*state->data;
			} break;

			case '[':
			{
				if(!*state->data && !matchBracket(state, true))
					return E_OPEN_BRACKET;
			} break;

			case ']':
			{
				if(*state->data && !matchBracket(state, false))
					return E_CLOSE_BRACKET;
			} break;

			case ',': *state->data = (int8_t)getchar(); break;
			case '.': putchar(*state->data); break;

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

/**
 * Move code pointer to corresponding bracket
 * @param state State to be modified
 * @param forward If true, move forward while searching, otherwise move backwards
 * @return True if match was found, false otherwise
 */
bool matchBracket(InterpreterState* state, bool forward)
{
	uint8_t* codeCurrent = state->code;

	// loop until matching bracket was found
	for(int i=0; i += (*state->code == '[') - (*state->code == ']'); state->code += (forward ? 1 : -1))
	{
		// check that we don't under/overflow
		if(state->code - state->codeStart < 0 || !*state->code)
		{
			state->code = codeCurrent;
			return false;
		}
	}

	return true;
}

/**
 * Find the position (row and column) of the code pointer
 * @param state State to be searched
 * @param row Output row
 * @param column Output column
 */
void findPosition(InterpreterState* state, int* row, int* column)
{
	int x = 1, y = 1;

	for(int i=0; i < (state->code - state->codeStart); ++i)
	{
		++x;

		if(state->codeStart[i] == '\n')
		{
			x = 1;
			++y;
		}
	}

	*row = y;
	*column = x;
}
