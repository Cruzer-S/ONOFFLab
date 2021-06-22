#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <inttypes.h>

#include <unistd.h>

#include <sys/socket.h>
#include <arpa/inet.h>

struct header_data {
	uint32_t id;			// 4
	uint8_t passwd[32];		// 36
	uint8_t method;			// 37
	uint8_t quality;		// 38
	uint32_t filesize;		// 42
	uint32_t checksum;		// 46

	uint8_t dummy[1024 - 46];
} __attribute__((packed));

uint32_t calc_checksum(struct header_data *data)
{
	uint32_t checksum = 0;

	checksum ^= data->id;
	checksum ^= data->passwd[0];
	checksum ^= data->filesize;
	checksum ^= data->quality;
	checksum ^= data->method;

	return checksum;
}

int main(int argc, char *argv[])
{
	struct header_data packet;
	FILE *fp;
	uint32_t u32;
	uint8_t u8;
	uint64_t u64;
	int what;

	if (argc < 2) {
		fprintf(stderr, "usage: %s <filename>\n", argv[0]);
		exit(EXIT_FAILURE);
	} else if (argc == 3) {
		what = strtol(argv[2], NULL, 10);
	} else {
		printf("what? ");
		printf("0. make packet, 1. read packet, 2. send packet\n");
		scanf("%d", &what);
	}

	switch (what) {
	case 0: // make packet
		printf("id: ");
		scanf("%" PRIu32, &u32);
		packet.id = u32;

		uint8_t u8_32[32];
		printf("passwd: ");
		scanf("%s", (char *) u8_32);
		memcpy(packet.passwd, u8_32, 32);

		printf("method: ");
		scanf("%" PRIu8, &u8);
		packet.method = u8;

		printf("quality: ");
		scanf("%" PRIu8, &u8);
		packet.quality = u8;

		printf("filesize: ");
		scanf("%" PRIu64, &u64);
		packet.filesize = u64;

		printf("checksum: ");
		if (scanf("%" PRIu32, &u32) != 0) {
			packet.checksum = u32;
		} else {
			while (getchar() != '\n') ;
			packet.checksum = calc_checksum(&packet);
			printf("caculated checksum: %" PRIu32 "\n",
					packet.checksum);
		}

		fp = fopen(argv[1], "wb");
		if (fp == NULL) {
			fprintf(stderr, "failed to open file: %s\n", strerror(errno));
			exit(EXIT_FAILURE);
		}

		if (fwrite(&packet, sizeof(struct header_data), 1, fp) != 1) {
			fprintf(stderr, "failed to write data to file\n");
			exit(EXIT_FAILURE);
		}

		do {
			uint8_t *body = malloc(packet.filesize);
			if (body == NULL)
				exit(EXIT_FAILURE);

			if (fwrite(body, packet.filesize, 1, fp) != 1) {
				fprintf(stderr, "failed to write file from data\n");
				exit(EXIT_FAILURE);
			}
		} while (false);

		fclose(fp);
		break;

	case 1: // read packet 
		fp = fopen(argv[1], "rb");
		if (fp == NULL)
			fprintf(stderr, "failed to open file: %s\n", strerror(errno));

		if (fread(&packet, sizeof(struct header_data), 1, fp) != 1) {
			fprintf(stderr, "failed to read data from file\n");
			exit(EXIT_FAILURE);
		}
		
		printf("id: %" PRIu32 "\n", packet.id);
		printf("passwd: %s\n", packet.passwd);
		printf("request: %" PRIu8 "\n", packet.method);
		printf("quality: %" PRIu8 "\n", packet.quality);
		printf("filesize: %" PRIu32 "\n", packet.filesize);
		printf("checksum: %" PRIu32 "\n", packet.checksum);

		fclose(fp);
		break;

	case 2: // send packet
		fp = fopen(argv[1], "rb");
		if (fp == NULL)
			fprintf(stderr, "failed to open file: %s\n", strerror(errno));

		int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (sock == -1)
			fprintf(stderr, "failed to socket(): %s", strerror(errno));

		struct sockaddr_in sock_adr;
		sock_adr.sin_family = AF_INET;
		sock_adr.sin_port = htons(1584);
		sock_adr.sin_addr.s_addr = inet_addr("127.0.0.1");
		if (connect(sock, (struct sockaddr *) &sock_adr, sizeof(sock_adr)) == -1)
			fprintf(stderr, "failed to connect(): %s", strerror(errno));

		uint8_t buffer[BUFSIZ];
		size_t read;
		while ((read = fread(&buffer, 1, BUFSIZ, fp)) >  0)
			if (write(sock, &buffer, read) == -1)
				fprintf(stderr, "failed to write(): %s",
						strerror(errno));

		sleep(5);

		close(sock);
		fclose(fp);

		break;
	}

	return 0;
}
