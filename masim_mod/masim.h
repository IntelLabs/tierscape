#ifndef _MASIM_H
#define _MASIM_H




#include <argp.h>
#include <err.h>
#include <fcntl.h>
#include <locale.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

struct mregion {
	char name[256];
	size_t sz;
	char *data;
};

enum rw_mode {
	READ_ONLY,
	WRITE_ONLY,
	READ_WRITE,
};

struct access {
	struct mregion *mregion;
	int random_access;
	size_t stride;
	int probability;
	enum rw_mode rw_mode;

	/* For runtime only */
	int prob_start;
	size_t last_offset;
	uint64_t last_page;
};

struct phase {
	char *name;
	unsigned time_ms;
	struct access *patterns;
	int nr_patterns;

	/* For runtime only */
	int total_probability;
};

#endif /* _MASIM_H */
