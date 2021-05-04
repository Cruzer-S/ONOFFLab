#ifndef LOGGER_H__
#define LOGGER_H__

#include <stdio.h>		// fprintf, stdout, stderr
#include <stdbool.h>	// false
#include <stdlib.h>		// exit(), EXIT_FAILURE

#define print_message(FPTR, TYPE, FMT, ...)					\
	fprintf(FPTR, "["#TYPE"] [%s/%s:%03d] " FMT "\n", 		\
			__FILE__, __FUNCTION__, __LINE__, ## __VA_ARGS__)

#define pr_out(FMT, ...) print_message(stdout, INF, FMT, __VA_ARGS__)
#define pr_err(FMT, ...) print_message(stderr, ERR, FMT, __VA_ARGS__)
#define pr_crt(FMT, ...) print_message(stderr, CRI, FMT, __VA_ARGS__), exit(EXIT_FAILURE)

#endif
