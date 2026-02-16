#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CLOWNMD5_IMPLEMENTATION
#define CLOWNMD5_STATIC
#include "clownmd5.h"

typedef unsigned char Hash[16];

static void HashFile(FILE* const file, Hash hash)
{
	ClownMD5_State state;
	unsigned char buffer[16 * 4];

	ClownMD5_Init(&state);

	for (;;)
	{
		const size_t bytes_read = fread(buffer, 1, sizeof(buffer), file);

		if (bytes_read != sizeof(buffer))
		{
			ClownMD5_PushFinalData(&state, buffer, bytes_read * 8, hash);
			break;
		}

		ClownMD5_PushData(&state, buffer);
	}
}

static int HashFromString(const char* const string, Hash hash)
{
	unsigned int values[16];
	unsigned int i;

	const int values_read = sscanf(string, "%2X%2X%2X%2X%2X%2X%2X%2X%2X%2X%2X%2X%2X%2X%2X%2X",
		&values[0],
		&values[1],
		&values[2],
		&values[3],
		&values[4],
		&values[5],
		&values[6],
		&values[7],
		&values[8],
		&values[9],
		&values[10],
		&values[11],
		&values[12],
		&values[13],
		&values[14],
		&values[15]
	);

	if (values_read != 16)
		return 0;

	for (i = 0; i < 16; ++i)
		hash[i] = values[i];

	return 1;
}

#ifndef FUZZER

int main(const int argc, char** const argv)
{
	int exit_code = EXIT_FAILURE;

	if (argc < 3)
	{
		fprintf(stderr, "Usage: %s filename md5hash\n", argv[0]);
		exit_code = EXIT_SUCCESS;
	}
	else
	{
		Hash expected_hash;

		if (!HashFromString(argv[2], expected_hash))
		{
			fputs("Could not parse hash.\n", stderr);
		}
		else
		{
			const char* const file_path = argv[1];
			FILE* const file = fopen(file_path, "rb");

			if (file == NULL)
			{
				fprintf(stderr, "Could not open file '%s'.\n", file_path);
			}
			else
			{
				Hash hash;

				HashFile(file, hash);

				fclose(file);

				if (memcmp(hash, expected_hash, sizeof(Hash)) == 0)
					exit_code = EXIT_SUCCESS;
			}
		}
	}

	if (__lsan_do_recoverable_leak_check() > 0) { abort(); }
	return exit_code;
}

#else

#include <openssl/md5.h>
#include <stdio.h>
#include <string.h>

// Extension or C99, so since we compile with -ansi we need to declare those ourselves here
FILE *fmemopen(void *buf, size_t size, const char *mode);
int snprintf(char *str, size_t size, const char *format, ...);
size_t strnlen(const char *s, size_t maxlen);

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	Hash expected_hash, hash_from_string, hash_from_file;
	char hash_string[16 * 2 + 1];
	FILE *memory_file;

	MD5(data, size, expected_hash);

	if (snprintf(hash_string, sizeof(hash_string), "%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X",
		expected_hash[0],
		expected_hash[1],
		expected_hash[2],
		expected_hash[3],
		expected_hash[4],
		expected_hash[5],
		expected_hash[6],
		expected_hash[7],
		expected_hash[8],
		expected_hash[9],
		expected_hash[10],
		expected_hash[11],
		expected_hash[12],
		expected_hash[13],
		expected_hash[14],
		expected_hash[15]
	) != (sizeof(hash_string) - 1))
		abort();

	if (!HashFromString(hash_string, hash_from_string))
		abort();
	if (memcmp(expected_hash, hash_from_string, sizeof(Hash)) != 0)
		abort();

	if (strnlen((char*)data, size) < size)
	{
		Hash garbage_hash;
		HashFromString(data, garbage_hash);
	}

	memory_file = fmemopen((void*)data, size, "rb");
	if (memory_file == NULL)
		abort();

	HashFile(memory_file, hash_from_file);
	fclose(memory_file);
	if (memcmp(expected_hash, hash_from_file, sizeof(Hash)) != 0)
		abort();

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
