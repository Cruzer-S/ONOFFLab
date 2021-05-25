#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <inttypes.h>

struct __attribute__((packed)) client_packet {
	uint32_t id;
	uint8_t passwd[32];

	uint8_t request;

	uint8_t quality;

	uint64_t filesize;

	uint32_t checksum;

	uint8_t dummy[1024 - 50];
};

int main(int argc, char *argv[])
{
	struct client_packet packet;
	FILE *fp;
	uint32_t u32;
	uint8_t u8;
	uint64_t u64;

	if (argc != 2) {
		fprintf(stderr, "usage: %s <filename>\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	printf("id: ");
	scanf("%" PRIu32, &u32);
	packet.id = u32;

	uint8_t u8_32[32];
	printf("passwd: ");
	scanf("%s", (char *) u8_32);
	memcpy(packet.passwd, u8_32, 32);

	printf("request: ");
	scanf("%" PRIu8, &u8);
	packet.request = u8;

	printf("quality: ");
	scanf("%" PRIu8, &u8);
	packet.quality = u8;

	printf("filesize: ");
	scanf("%" PRIu64, &u64);
	packet.filesize = u64;

	printf("checksum: ");
	scanf("%" PRIu32, &u32);
	packet.checksum = u32;

	fp = fopen(argv[1], "wb");
	if (fp == NULL)
		fprintf(stderr, "failed to open file: %s\n", strerror(errno));

	if (fwrite(&packet, sizeof(struct client_packet), 1, fp) != 1) {
		fprintf(stderr, "failed to write file from data\n");
		exit(EXIT_FAILURE);
	}

	do {
		uint8_t body[packet.filesize];
		if (fwrite(&body, sizeof(body), 1, fp) != 1) {
			fprintf(stderr, "failed to write file from data\n");
			exit(EXIT_FAILURE);
		}
	} while (false);

	fclose(fp);

	return 0;
}
