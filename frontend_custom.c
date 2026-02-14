/*
 * Copyright (C) 2022-2025 Clownacy
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "clowncommon/clowncommon.h"

#include "semantic.h"

#define ERROR(message) do { fputs("Error: " message "\n", stderr); exit_code = EXIT_FAILURE;} while (0)

static int total_arguments;
static char **arguments;

static void DefinitionCallback(void *internal, void* const user_data, const ClownAssembler_AddDefinition add_definition)
{
	int i;

	(void)user_data;

	for (i = 1; i < total_arguments; ++i)
	{
		if (arguments[i][0] == '-' && arguments[i][1] == 'e')
		{
			const char* const identifier = arguments[i + 1];
			const size_t identifier_length = strcspn(identifier, "=");

			unsigned long value;
			char *str_end;

			if (identifier[identifier_length] != '=')
			{
				value = 0; /* Exactly what asm68k does... */
			}
			else
			{
				value = strtoul(&identifier[identifier_length + 1], &str_end, 0);

				if (str_end < strchr(identifier, '\0'))
					fprintf(stderr, "Error: Value of argument '-e %s' is invalid.\n", arguments[i + 1]);
			}

			add_definition(internal, identifier, identifier_length, value);

			++i; /* Skip argument. */
		}
	}
}

#ifndef FUZZER

int main(int argc, char **argv)
{
	int exit_code = EXIT_SUCCESS;

	cc_bool print_usage;
	const char *input_file_path;
	const char *output_file_path;
	const char *listing_file_path;
	const char *symbol_file_path;
	ClownAssembler_Settings settings = {0};
	int i;

	total_arguments = argc;
	arguments = argv;

	print_usage = cc_false;
	input_file_path = NULL;
	output_file_path = NULL;
	listing_file_path = NULL;
	symbol_file_path = NULL;
	settings.local_signifier = '@';
	settings.case_insensitive = cc_false;
	settings.debug = cc_false;
	settings.equ_set_descope_local_labels = cc_false;
	settings.output_local_labels_to_sym_file = cc_false;
	settings.warnings_enabled = cc_true;
	settings.pedantic_warnings_enabled = cc_true;
	settings.expand_all_macros = cc_false;
	settings.automatic_even = cc_false;

	for (i = 1; i < argc; ++i)
	{
		if (argv[i][0] == '-')
		{
			switch (argv[i][1])
			{
				case 'h':
					print_usage = cc_true;
					continue;

				case 'i':
					if (i < argc && argv[i + 1][0] != '-')
					{
						++i;
						input_file_path = argv[i];
					}

					continue;

				case 'o':
					if (i < argc && argv[i + 1][0] != '-')
					{
						++i;
						output_file_path = argv[i];
					}

					continue;

				case 'l':
					if (i < argc && argv[i + 1][0] != '-')
					{
						++i;
						listing_file_path = argv[i];
					}

					continue;

				case 's':
					if (i < argc && argv[i + 1][0] != '-')
					{
						++i;
						symbol_file_path = argv[i];
					}

					continue;

				case 'a':
					settings.automatic_even = cc_true;
					continue;

				case 'c':
					settings.case_insensitive = cc_true;
					continue;

				case 'b':
					settings.debug = cc_true;
					continue;

				case 'd':
					settings.equ_set_descope_local_labels = cc_true;
					continue;

				case 'e':
					/* We'll deal with this later, in DefinitionCallback. */
					if (i < argc && argv[i + 1][0] != '-')
						++i;

					continue;

				case 'm':
					settings.expand_all_macros = cc_true;
					continue;

				case 'p':
					settings.pedantic_warnings_enabled = cc_false;
					continue;

				case 'v':
					settings.output_local_labels_to_sym_file = cc_true;
					continue;

				case 'w':
					settings.warnings_enabled = cc_false;
					continue;

				case '.':
					settings.local_signifier = argv[i][2];
					continue;
			}
		}

		fprintf(stderr, "Error: Unrecognised option '%s'.\n", argv[i]);
		exit_code = EXIT_FAILURE;
	}

	if (argc < 2 || print_usage)
	{
		fputs(
			CLOWNASSEMBLER_VERSION_STRING " - An assembler for the Motorola 68000.\n"
			"\n"
			"Options:\n"
			" -i [path] - Input file. If not specified, STDIN is used instead.\n"
			" -o [path] - Output file.\n"
			" -l [path] - Listing file. Optional.\n"
			" -s [path] - asm68k-style symbol file. Optional.\n"
			, stdout);
		fputs(
			" -a        - Enable automatic-even mode.\n"
			" -c        - Enable case-insensitive mode.\n"
			" -b        - Enable Bison's debug output.\n"
			" -d        - Allow EQU/SET to descope local labels.\n"
			" -e X=Y    - Defines symbol X to value Y.\n"
			" -p        - Silence pedantic warnings.\n"
			" -v        - Include local labels in symbol file.\n"
			" -w        - Silence warnings.\n"
			" -.X       - Set local label signifier to character after dot (default '@').\n"
			, stdout);
	}
	else
	{
		if (output_file_path == NULL)
		{
			ERROR("Output file path must be specified with '-o'.");
		}
		else
		{
			FILE *input_file;

			if (input_file_path == NULL)
				input_file = stdin;
			else
				input_file = fopen(input_file_path, "r");

			if (input_file == NULL)
			{
				ERROR("Could not open input file.");
			}
			else
			{
				FILE *output_file;

				output_file = fopen(output_file_path, "wb");

				if (output_file == NULL)
				{
					ERROR("Could not open output file.");
				}
				else
				{
					FILE *listing_file;
					FILE *symbol_file;
					cc_bool success;

					if (listing_file_path == NULL)
					{
						listing_file = NULL;
					}
					else
					{
						listing_file = fopen(listing_file_path, "w");

						if (listing_file == NULL)
							ERROR("Could not open listing file.");
					}

					if (symbol_file_path == NULL)
					{
						symbol_file = NULL;
					}
					else
					{
						symbol_file = fopen(symbol_file_path, "wb");

						if (symbol_file == NULL)
							ERROR("Could not open symbol file.");
					}

					success = ClownAssembler_AssembleFile(input_file, output_file, stderr, listing_file, symbol_file, input_file_path != NULL ? input_file_path : "STDIN", &settings, DefinitionCallback, NULL);

					if (symbol_file != NULL)
						fclose(symbol_file);

					if (listing_file != NULL)
						fclose(listing_file);

					fclose(output_file);

					if (!success)
					{
						ERROR("Could not assemble.");
						/* Delete the output file; it will be junk anyway.
						   Also, leaving a junk file will confuse Make, which will think that the assembler had previously succeeded. */
						remove(output_file_path);
					}
				}

				fclose(input_file);
			}
		}
	}

	if (__lsan_do_recoverable_leak_check() > 0) { abort(); }
	return exit_code;
}

#else

struct MemoryTextInputState {
	const char *buffer;
	size_t position;
	size_t size;
};

/* Basically has to emulate fgets, but for a buffer in memory instead of a file, and with the same semantics as fgets (e.g. including the newline character, and null-terminating the buffer). */
static char *fuzzer_read_line(void *user_data, char *buffer, size_t buffer_size)
{
	struct MemoryTextInputState *state = (struct MemoryTextInputState*)user_data;
	size_t newline_position;
	size_t line_length;
	char *newline_ptr;

	if (state->position >= state->size)
		return NULL; /* EOF */

	newline_ptr = (char *)memchr(&state->buffer[state->position], '\n', state->size - state->position);
	if (newline_ptr == NULL)
		newline_position = state->size - state->position; /* No newline found, so the line goes until the end of the buffer. */
	else
		newline_position = (size_t)(newline_ptr - &state->buffer[state->position]);

	line_length = newline_position;

	if (state->position + newline_position < state->size)
		line_length += 1; /* Include the newline character if there is one. */

	if (line_length >= buffer_size)
		line_length = buffer_size - 1; /* Leave space for null terminator. */

	memcpy(buffer, &state->buffer[state->position], line_length);
	buffer[line_length] = '\0'; /* Null-terminate the buffer. */
	state->position += line_length;
	if (state->position < state->size && state->buffer[state->position] == '\n')
		state->position += 1; /* Skip the newline character. */

	return buffer;
}

static void fuzzer_print_formatted(void *user_data, const char *format, va_list args)
{
	(void)user_data;
	(void)format;
	(void)args;
	/* We don't actually need to do anything here since the fuzzer doesn't check the output, but we have to provide a function for this callback since the assembler will call it. */
}

static void fuzzer_write_character(void *user_data, int character)
{
	(void)user_data;
	(void)character;
	/* We don't actually need to do anything here since the fuzzer doesn't check the output, but we have to provide a function for this callback since the assembler will call it. */
}

static void fuzzer_write_string(void *user_data, const char *string)
{
	(void)user_data;
	(void)string;
	/* We don't actually need to do anything here since the fuzzer doesn't check the output, but we have to provide a function for this callback since the assembler will call it. */
}

/* we're compiling in pure C mode but use memmem for the tests anyway. Declare it manually */
void *memmem(const void *haystack, size_t haystacklen, const void *needle, size_t needlelen);

#include <ctype.h>

static cc_bool check_for_keywords_that_could_trigger_timeouts(const uint8_t *data, size_t size)
{
	const char *keywords[] = {
		"while",
		"rept",
		"macro",
	};
	size_t num_keywords = sizeof(keywords) / sizeof(keywords[0]);
	size_t i;

	for (i = 0; i < num_keywords; ++i) {
		const uint8_t *current_data = data;
		size_t current_size = size;

		while (cc_true) {
			char before, after;
			size_t keyword_length = strlen(keywords[i]);
			const uint8_t *memmem_result = (uint8_t *)memmem(current_data, current_size, keywords[i], keyword_length);
			size_t offset;
			if (memmem_result == NULL)
				break;
			offset = memmem_result - current_data;

			/* Check that it's actually a word */
			before = (memmem_result == data) ? ' ' : memmem_result[-1];
			after = (offset + keyword_length >= current_size) ? ' ' : memmem_result[keyword_length];
			if (!isalnum(before) && before != '_' && !isalnum(after) && after != '_')
				return cc_true;

			current_size -= offset + 1;
			current_data += offset + 1;
		}
	}

	return cc_false;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	ClownAssembler_TextInput input_callbacks;
	struct MemoryTextInputState input_state = {0};
	ClownAssembler_BinaryStream output_callbacks, symbol_callbacks;
	ClownAssembler_TextOutput text_output_callbacks;
	ClownAssembler_Settings settings = {0};

	if (cc_false && check_for_keywords_that_could_trigger_timeouts(data, size))
#ifdef LIBFUZZER
		return -1;
#else
		return 0; /* This returns 0 because we're using Honggfuzz - reexamine if this should be -1 under libFuzzer */
#endif

	input_state.buffer = (const char*)data;
	input_state.position = 0;
	input_state.size = size;

	input_callbacks.user_data = &input_state;
	input_callbacks.read_line = fuzzer_read_line;

	if (!BinaryStream_OpenMemory(&output_callbacks) || !BinaryStream_OpenMemory(&symbol_callbacks)) {
		abort();
	}

	text_output_callbacks.user_data = NULL;
	text_output_callbacks.print_formatted = fuzzer_print_formatted;
	text_output_callbacks.write_character = fuzzer_write_character;
	text_output_callbacks.write_string = fuzzer_write_string;

	settings.local_signifier = '@';
	settings.case_insensitive = size & 1;
	settings.debug = cc_false;
	settings.equ_set_descope_local_labels = size & 2;
	settings.output_local_labels_to_sym_file = cc_false;
	settings.warnings_enabled = cc_true;
	settings.pedantic_warnings_enabled = cc_true;
	settings.expand_all_macros = cc_false;
	settings.automatic_even = cc_false;

	ClownAssembler_Assemble(&input_callbacks, &output_callbacks, &text_output_callbacks, NULL, &symbol_callbacks, "fuzzer input", &settings, DefinitionCallback, NULL);

	BinaryStream_CloseMemory(&output_callbacks);
	BinaryStream_CloseMemory(&symbol_callbacks);

	if (__lsan_do_recoverable_leak_check() > 0) { abort(); }
	return 0;
}

#ifdef AFL

__AFL_FUZZ_INIT();

int main(void)
{
	unsigned char *buf = __AFL_FUZZ_TESTCASE_BUF;

	while (__AFL_LOOP(10000)) {
		int len = __AFL_FUZZ_TESTCASE_LEN;

		LLVMFuzzerTestOneInput(buf, len);
	}
}

#endif

#ifdef LIBFUZZER

int LLVMFuzzerRunDriver(int *argc, char ***argv, int (*UserCb)(const uint8_t *Data, size_t Size));

int main(int argc, char **argv)
{
	return LLVMFuzzerRunDriver(&argc, &argv, LLVMFuzzerTestOneInput);
}

#endif

#endif
