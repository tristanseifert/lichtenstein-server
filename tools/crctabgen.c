/**
 * Generates the look-up table used by the CRC32 generator. Output is written
 * to the standard output, which can be re-directed to a file to produce a
 * header that can be included in the CRC32 code.
 */
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

const uint32_t Polynomial = 0x04C11DB7;

const size_t MaxSlice = 16;
uint32_t Crc32Lookup[MaxSlice][256];

int main(int argc, char *argv[]) {
	// generate the table
	for(int i = 0; i <= 0xFF; i++) {
		uint32_t crc = i;
		for (int j = 0; j < 8; j++)
		crc = (crc >> 1) ^ ((crc & 1) * Polynomial);
		Crc32Lookup[0][i] = crc;
	}

	for(int i = 0; i <= 0xFF; i++) {
		// now, extend it for all 16 slices
		for(int slice = 1; slice < MaxSlice; slice++) {
			Crc32Lookup[slice][i] = (Crc32Lookup[slice - 1][i] >> 8) ^ Crc32Lookup[0][Crc32Lookup[slice - 1][i] & 0xFF];
		}
	}

	// output the table
	puts("#ifndef CRC32LOOKUP_H");
	puts("#define CRC32LOOKUP_H\n");
	puts("const uint32_t Crc32Lookup[MaxSlice][256] = {");

	for(int slice = 0; slice < MaxSlice; slice++) {
		puts("\t{");

		for(int i = 0; i <= 0xFF; i++) {
			if((i & 0x07) == 0x00) {
				printf("\t\t");
			}

			printf("0x%08X", Crc32Lookup[slice][i]);

			if(i != 0xFF) {
				printf(",");
			}

			if((i & 0x07) == 0x07) {
				printf("\n");
			}
		}

		if(slice != (MaxSlice - 1)) {
			puts("\t},");
		} else {
			puts("\t}");
		}
	}

	puts("};");
	puts("\n#endif");
}
