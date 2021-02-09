#include <stdio.h>
#include <stdlib.h>		// malloc, free
#include <string.h>		// strcat, strcpy
#include <stdarg.h>		// vfprintf
#include <unistd.h>		// FILENAME_MAX
#include <stdint.h>		// unit32_t
#include <stddef.h>		// ptrdiff_t
#include <stdbool.h>	// true, false

#define MAXLINE_LEN	BUFSIZ
#define ENDSTRING	"ENDCHAR"

// Error-Handling Function
_Noreturn void error_handling(const char *, ...);

// Process .BDF extension file
int process_bdf(
	// Original and Destination file
	FILE *original, FILE *destination, 

	// Processing Properties
	unsigned encoding_number_length, 
	unsigned bitmap_number_width, 
	unsigned number_of_line
);

// Convert Character to Unsigned int
ptrdiff_t ctou(char );

// Convert String To Unsinged int with 32-bit
uint32_t stou32(const char *numstr);

// Read line (stop reading when reaching the new-line or EOF character)
// Return Value: -1 when reaching EOF, otherwise, greater or equal than 0.
//               Returning negative value when some error is occured.
int readline(FILE *fp, char *line, int maxlen);

int main(int argc, char *argv[])
{
	FILE *orig, *dest;

	if (argc != 5)
		error_handling("usage: <%s> <filename> <encoding> <length> <nline>\n", argv[0]);

	orig = fopen(argv[1], "r");
	if (orig == NULL) {
		error_handling("failed to open file %s\n", argv[1]);
	} else {
		char *filename = malloc(sizeof(char) * FILENAME_MAX);
	
		// {filename} to {filename}.out
		strcat(strcpy(filename, argv[1]), ".out");

		// create {filename}.out file
		dest = fopen(filename, "w");
		if (dest == NULL) {
			fclose(orig);
			error_handling("failed to create file %s\n", filename);
		}

		free(filename);
	}

	// enlen = Encoding number Length	(Second argument, argv[2])
	// width = Bitmap number Width		(Third ...)
	// nlen  = Number of line			(Fourth ...)
	for (unsigned enlen = atoi(argv[2]), width = atoi(argv[3]), nlen = atoi(argv[4]);
		 process_bdf(orig, dest, enlen, width, nlen) > 0; ) 
		/* empty loop body */ ;

	fclose(orig);
	fclose(dest);

	return 0;
}

_Noreturn void error_handling(const char *msg, ...)
{
	va_list ap;

	va_start(ap, msg);

	vfprintf(stderr, msg, ap);

	va_end(ap);

	exit(1);
}

int readline(FILE *fp, char *line, int maxlen)
{
	char *const orig = line;
	int ch;

	while ((ch = fgetc(fp)) != EOF 
		&& ch != '\n' && line - orig < maxlen)
		*line++ = ch;

	*line = '\0';

	return (ch == EOF) ? -1 : line - orig;
}

int process_bdf(FILE *orig, FILE *dest,
				unsigned enlen, unsigned width, unsigned nlen)
{
	char line[MAXLINE_LEN + 1];

	// STARTCHAR (Skip and Test EOF)
	if (readline(orig, line, MAXLINE_LEN) == -1)
		return 0;

	// ENCODING (Extract)
	readline(orig, line, MAXLINE_LEN);
	
	/* Parse Encoding number */ {
		int value;

		sscanf(line, "%*s %d", &value);
		if (fprintf(dest, "%.*d\n", enlen, value) != (enlen + 1))
			return -1;
	}

	// SWIDTH (Skip)
	readline(orig, line, MAXLINE_LEN);

	// DWIDTH (Skip)
	readline(orig, line, MAXLINE_LEN);

	// BBX (Skip)
	readline(orig, line, MAXLINE_LEN);

	// BITMAP (Skip)
	readline(orig, line, MAXLINE_LEN);

	/* Processing BITMAP data */ {
		int read_line = 0;
		uint32_t bitmap;

		while (read_line++ <= nlen)
		{
			readline(orig, line, MAXLINE_LEN);

			if (!strcmp(line, ENDSTRING))
				break;

			bitmap = stou32(line);

			fprintf(dest, "%.*X\n", width, bitmap);
		}

		while (read_line++ <= nlen)
			if (fprintf(dest, "%.*d\n", width, 0x00) != (width + 1))
				return -2;
	}

	return 1;
} 

ptrdiff_t ctou(char c)
{
	const char *find = "0123456789ABCDEF";

	return (strchr(find, c) - find);
}

uint32_t stou32(const char *numstr)
{
	uint32_t ret = 0;

	while (*numstr)
		ret = (ret * 16) + ctou(*numstr++);

	return ret;
}
