#define _GNU_SOURCE

#include "masim.h"

#include "misc.h"

//#define OPS_MODE
#define AUTO_MODE
//#define PIN_MODE

#define LEN_ARRAY(x) (sizeof(x) / sizeof(*x))

#define SZ_PAGE 4096

void print_memory_status();

enum hintmethod {
    NONE,
    MADVISE,
    MLOCK,
};

enum hintmethod hintmethod = NONE;
int quiet;
int skip_asm = 0;
enum rw_mode default_rw_mode = READ_WRITE;

void* overload_malloc(size_t size) {
    void* ptr;
    if( size > 1000){
        fprintf(stderr, "malloc %zu MB \n", size / 1024 / 1024);
    }

    ptr = malloc(size);
    if (ptr == NULL) {
        fprintf(stderr, "malloc failed\n");
        exit(1);
    }
    return ptr;
}

/* Print the memory regions. */
void pr_regions(struct mregion* regions, size_t nr_regions) {
    struct mregion* region;
    int i;

    fprintf(stderr, "memory regions\n");
    for (i = 0; i < nr_regions; i++) {
        region = &regions[i];
        fprintf(stderr, "\t%s: %zu bytes\n", region->name, region->sz);
    }
    fprintf(stderr, "\n");
}

/* Print phase information */
void pr_phase(struct phase* phase) {
    struct access* pattern;
    int j;
#ifdef OPS_MODE
    fprintf(stderr, "Phase (%s) for %u ops \n", phase->name, phase->time_ms);
#else
    fprintf(stderr, "Phase (%s) for %u ms \n", phase->name, phase->time_ms);
#endif
    for (j = 0; j < phase->nr_patterns; j++) {
        pattern = &phase->patterns[j];
        fprintf(stderr, "\tPattern %d ", j);
        fprintf(stderr, " %s access region %s with stride %zu mode %d (0:ro 1:wo 2:rw)\n",
            pattern->random_access ? "randomly" : "sequentially",
            pattern->mregion == NULL ? "..." : pattern->mregion->name,
            pattern->stride,
            pattern->rw_mode);
    }
}

void pr_phases(struct phase* phases, int nr_phases) {
    int i;

    for (i = 0; i < nr_phases; i++)
        pr_phase(&phases[i]);
}

struct access_config {
    struct mregion* regions;
    ssize_t nr_regions;
    struct phase* phases;
    ssize_t nr_phases;
};

#define RAND_BATCH 1000
#define RAND_ARR_SZ 1000
int rndints[RAND_BATCH][RAND_ARR_SZ];

static void init_rndints(void) {
    int i, j;

    for (i = 0; i < RAND_BATCH; i++)
        for (j = 0; j < RAND_ARR_SZ; j++)
            rndints[i][j] = rand();
    rndints[0][0] = 1;
}

/*
 * Returns a random integer
 */
static int rndint(void) {
    static int rndofs;
    static int rndarr;

    if (rndofs == RAND_ARR_SZ) {
        rndarr = rand() % RAND_BATCH;
        rndofs = 0;
    }

    return rndints[rndarr][rndofs++];
}

static void do_rnd_ro(struct access* access, int batch) {
    struct mregion* region = access->mregion;
    char* rr = region->data;
    int i;
    char __attribute__((unused)) read_val;

    for (i = 0; i < batch; i++)
        read_val = ACCESS_ONCE(rr[rndint() % region->sz]);
}

// macro page_align
// #define PAGE_ALIGN(addr) ((addr) + SZ_PAGE - ((addr) % SZ_PAGE))
#define PAGE_ALIGN(value) ((value) & ~((1 << 12) - 1))


static void do_seq_ro(struct access* access, int nr_pages) {
    struct mregion* region = access->mregion;
    char* rr = region->data;
    // int i;
    size_t offset = 0;
    // uint64_t local_tmp = 0;
    char __attribute__((unused)) read_val;

    int accessed_pages = 0;
    uint64_t last_page = 0;

    // for (i = 0; i < nr_pages; i++) 
    while (accessed_pages < nr_pages) {
        {
            offset += access->stride;
            if (offset >= region->sz) {
                // fprintf(stderr, "offset: %ld, region size: %ld\n", offset, region->sz);
                offset = 0;
            }
            read_val = ACCESS_ONCE(rr[offset]);

            if (last_page != PAGE_ALIGN(offset)) {
                last_page = PAGE_ALIGN(offset);
                // local_tmp++;
                accessed_pages++;

                // // print every 1000 accessed_page
                // if (accessed_pages % 9999 == 0) {
                //     fprintf(stderr, "Accessed page: %ld offset %ld page aligned address: %p\n", accessed_pages, offset, PAGE_ALIGN(offset));
                // }
            }
        }
    }
}



/* static void do_seq_ro(struct access* access, int batch) {
    struct mregion* region = access->mregion;
    char* rr = region->data;
    size_t offset = access->last_offset;
    uint64_t last_page = access->last_page;
    int i;
    char __attribute__((unused)) read_val;
    uint64_t local_tmp = 0;

    for (i = 0; i < batch; i++) {
        offset += access->stride;
        if (offset >= region->sz) {
            // fprintf(stderr, "offset: %ld, region size: %ld\n", offset, region->sz);
            offset = 0;
        }
        else {
            if (last_page != PAGE_ALIGN(offset)) {
                // fprintf(stderr, "page: %p cnt: %ld offset: %p last_offset %p\n", (void*)last_page, local_tmp, offset, offset-access->stride);
                last_page = PAGE_ALIGN(offset);
                local_tmp++;
            }

        }
        read_val = ACCESS_ONCE(rr[offset]);
        // char* page_selected = rr + offset;
        // read_val = ACCESS_ONCE(page_selected[rndint() % SZ_PAGE]);
    }
    // fprintf(stderr,"local_tmp is %ld\n", local_tmp);
    access->last_offset = offset;
    access->last_page = PAGE_ALIGN(last_page);
}
 */
static void do_rnd_wo(struct access* access, int batch) {
    struct mregion* region = access->mregion;
    char* rr = region->data;
    int i;

    for (i = 0; i < batch; i++)
        ACCESS_ONCE(rr[rndint() % region->sz]) = 1;
}

static void do_seq_wo(struct access* access, int batch) {
    struct mregion* region = access->mregion;
    char* rr = region->data;
    size_t offset = access->last_offset;
    int i;

    for (i = 0; i < batch; i++) {
        offset += access->stride;
        if (offset >= region->sz)
            offset = 0;
        ACCESS_ONCE(rr[offset]) = 1;
    }
    access->last_offset = offset;
}

static void do_rnd_rw(struct access* access, int batch) {
    struct mregion* region = access->mregion;
    char* rr = region->data;
    int i;
    char read_val;

    for (i = 0; i < batch; i++) {
        size_t rndoffset;

        rndoffset = rndint() % region->sz;
        read_val = ACCESS_ONCE(rr[rndoffset]);
        ACCESS_ONCE(rr[rndoffset]) = read_val + 1;
    }
}

static void do_seq_rw(struct access* access, int batch) {
    struct mregion* region = access->mregion;
    char* rr = region->data;
    size_t offset = access->last_offset;
    int i;
    char read_val;

    for (i = 0; i < batch; i++) {
        offset += access->stride;
        if (offset >= region->sz)
            offset = 0;
        read_val = ACCESS_ONCE(rr[offset]);
        ACCESS_ONCE(rr[offset]) = read_val + 1;
    }
    access->last_offset = offset;
}

static unsigned long long do_access(struct access* access, int batch) {

    switch (access->rw_mode) {
    case READ_ONLY:
        if (access->random_access)
            do_rnd_ro(access, batch);
        else
            do_seq_ro(access, batch);
        break;
    case WRITE_ONLY:
        if (access->random_access)
            do_rnd_wo(access, batch);
        else
            do_seq_wo(access, batch);
        break;
    case READ_WRITE:
        if (access->random_access)
            do_rnd_rw(access, batch);
        else
            do_seq_rw(access, batch);
        break;
    default:
        break;
    }

    return batch;
}


void hint_access_pattern(struct phase* phase) {
    static const unsigned MEMSZ_OFFSET = 10 * 1024 * 1024; /* 10 MB */
    static const unsigned FREQ_OFFSET = 70;                /* 70 % */

    struct access* acc;
    struct mregion* region;
    int freq_offset;
    int i;

    if (hintmethod == MLOCK)
        munlockall();

    freq_offset = phase->total_probability * FREQ_OFFSET / 100;

    for (i = 0; i < phase->nr_patterns; i++) {
        acc = &phase->patterns[i];
        region = acc->mregion;

        if (region->sz < MEMSZ_OFFSET)
            continue;

        if (acc->probability < freq_offset)
            continue;

        if (hintmethod == MLOCK) {
            if (mlock(region->data, region->sz) == -1)
                err(-1, "failed mlock");
        }
        else if (hintmethod == MADVISE) {
            /* madvise receive page size aligned region only */
            uintptr_t aligned;

            aligned = (uintptr_t)region->data;
            if (aligned % SZ_PAGE != 0)
                aligned = aligned + SZ_PAGE -
                (aligned % SZ_PAGE);

            if (madvise((void*)aligned, region->sz,
                MADV_WILLNEED) == -1)
                err(-1, "failed madvise");
        }
    }
}

void print_memory_status() {
    char cmd[200];
    sprintf(cmd, "cat /proc/%d/status |grep \'VmRSS\\|VmData\\|VmSwap\'", getpid());
    system(cmd);
    printf("\n");
}

void exec_phase(struct phase* phase) {
    struct access* pattern;
    unsigned long long nr_access;
    int randn;
    size_t i;

    struct timeval fstop, fstart;

#ifdef OPS_MODE
    uint32_t total_loops = phase->time_ms;
    // print nr_patterns

#else
    static unsigned long long cpu_cycle_ms;
    unsigned long long start;

    if (!cpu_cycle_ms)
        cpu_cycle_ms = aclk_freq() / 1000;

    start = aclk_clock();

#endif
    int total_pages = phase->patterns[0].mregion->sz / 4096;
    fprintf(stderr, "Total pages: %d\n", total_pages);
    nr_access = 0;
    gettimeofday(&fstart, NULL);

    if (hintmethod != NONE)
        hint_access_pattern(phase);

#ifndef OPS_MODE
    while (1)
#else
    while (total_loops-- > 0)
#endif
    {
        randn = rndint() % phase->total_probability;
        for (i = 0; i < phase->nr_patterns; i++) {
            int prob_start, prob_end;

            pattern = &phase->patterns[i];
            prob_start = pattern->prob_start;
            prob_end = prob_start + pattern->probability;
            if (randn >= prob_start && randn < prob_end)
                nr_access += do_access(pattern, total_pages);
        }

#ifndef OPS_MODE
        if (aclk_clock() - start > cpu_cycle_ms * phase->time_ms)
            break;
#endif
    }

    gettimeofday(&fstop, NULL);
    fprintf(stderr, "%s   REGION_TIME %lu us. Total Ops: %llu\n",
#ifdef OPS_MODE
        "OPS_MODE",
#else
        "TIME_MODE",
#endif
        (fstop.tv_sec - fstart.tv_sec) * 1000000 + fstop.tv_usec - fstart.tv_usec, nr_access);

    // print region time in seconds
    fprintf(stderr, "REGION_TIME_SEC %f\n", (fstop.tv_sec - fstart.tv_sec) + (fstop.tv_usec - fstart.tv_usec) / 1000000.0);

    // print_memory_status();
}

#include <stdio.h>
#include <stdlib.h>

#define NR_CPU 176

unsigned long region_addr;
unsigned long region_size;

// void* pthread_alloc(void* arg) {
//     unsigned long end_addr, block, thd_id;
//     long* addr;

//     thd_id = (unsigned long)arg;

//     block = region_size / NR_CPU;
//     addr = (long*)(region_addr + (block * thd_id));
//     end_addr = (unsigned long)addr + block;

//     // fprintf(stderr, "TID: %ld Region: 0x%016lx to 0x%016lx block: %ld, size: %ld\n", thd_id, (long unsigned int)addr, end_addr, block, region_size);

// #if 1
//     while ((unsigned long)addr < end_addr) {
//         *addr = 0xDEAD;
//         addr = (long*)((unsigned long)addr + 4096);
//     }
// #endif

//     pthread_exit(0);
// }

void init_memory_regions(struct access_config* config) {
    struct mregion* region;
    size_t i;

    unsigned long c = 0;
    void* ret;
    pthread_t thid;
    for (i = 0; i < config->nr_regions; i++) {
        region = &config->regions[i];
        // region->data = (char *)overload_malloc(region->sz);
        // region->sz = PAGE_ALIGN(region->sz);

        //ensure region->sz is a multiple of SZ_PAGE
        if (region->sz % SZ_PAGE != 0) {
            // fprintf(stderr, "Region %lu size is not page aligned: %lu\n", i, region->sz);
            // make it page_aligned
            region->sz = region->sz + SZ_PAGE - (region->sz % SZ_PAGE);
        }

    /*     region->data = (char*)mmap(NULL, region->sz, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
        if (region->data == MAP_FAILED) {
            perror("Could not mmap");
            exit(1);
        }
 */
        region->data = (char*)overload_malloc(region->sz);
        if (region->data == NULL) {
            perror("malloc");
            exit(1);
        }

        // ensure that region->data is PAGE_ALIGNED and print if it was not earlier
        if ((unsigned long)region->data % SZ_PAGE != 0) {
            // fprintf(stderr, "Region %lu address is not page aligned: %p\n", i, (void*)region->data);
            // // make it page_aligned
            region->data = (char*)((unsigned long)region->data + SZ_PAGE - ((unsigned long)region->data % SZ_PAGE));
        }
    }

    for (i = 0; i < config->nr_regions; i++) {
        region = &config->regions[i];


        region_addr = (unsigned long)region->data;
        region_size = (unsigned long)region->sz;

        // for (c = 0; c < NR_CPU; c++) {
        //     sleep(0.1);
        //     if (pthread_create(&thid, NULL, pthread_alloc, (void*)c) != 0) {
        //         perror("pthread_create() error");
        //         exit(1);
        //     }
        // }

        // if (pthread_join(thid, &ret) != 0) {
        //     perror("pthread_create() error");
        //     exit(3);
        // }

         memset(region->data, 1, region->sz);
    //     // fprintf(stderr, "Region %lu address is %p : %p\n", i, (void*)region->data, (void*)region->data + region->sz);
    }

    // // maintain minimum and maximum address of regions
    // unsigned long min_addr = (unsigned long)config->regions[0].data;
    // unsigned long max_addr = (unsigned long)config->regions[0].data + config->regions[0].sz;

    for (i = 0; i < config->nr_regions; i++) {
        region = &config->regions[i];
        fprintf(stderr, "Region %lu address is %p : %p\n", i, (void*)region->data, (void*)region->data + region->sz);

        // // update min_addr and max_addr
        // if ((unsigned long)region->data < min_addr)
        //     min_addr = (unsigned long)region->data;
        // if ((unsigned long)region->data + region->sz > max_addr)
        //     max_addr = (unsigned long)region->data + region->sz;
    }

    // // write min_addr and max_addr to /tmp/masim_addr
    // FILE* addr_file = fopen("/tmp/masim_addr", "w");
    // if (addr_file == NULL) {
    //     fprintf(stderr, "Could not open file /tmp/masim_addr\n");
    //     // exit(1);
    // }
    // else {


    //     fprintf(addr_file, "%p %p\n", (void*)min_addr, (void*)max_addr);
    //     // fprintf(addr_file, "max_addr: %p\n", (void*)max_addr);
    //     fclose(addr_file);
    // }

    fprintf(stderr, "Memory allocations done.\n");
    fprintf(stderr, "PID is: %d\n", getpid());

    print_memory_status();
}

void exec_config(struct access_config* config) {
    // struct mregion* region;
    size_t i;
    // sigset_t signal_set;
    // int sig_number;
    // unsigned long c = 0;
    // void* ret;
    // pthread_t thid;



    for (i = 0; i < config->nr_phases; i++) {
        fprintf(stderr, "Executing phase %ld/%ld\n", i, config->nr_phases);
        pr_phase(&config->phases[i]);
        exec_phase(&config->phases[i]);
    }

    fprintf(stderr, "Executions done.\n");
    fprintf(stderr, "PID is: %d\n", getpid());
}

size_t len_line(char* str, size_t lim_seek) {
    size_t i;

    for (i = 0; i < lim_seek; i++) {
        if (str[i] == '\n')
            break;
    }

    if (i == lim_seek)
        return -1;

    return i;
}

/* Read exactly required bytes from a file */
void readall(int file, char* buf, ssize_t sz) {
    ssize_t sz_read;
    ssize_t sz_total_read = 0;
    while (1) {
        sz_read = read(file, buf + sz_total_read, sz - sz_total_read);
        if (sz_read == -1)
            err(1, "read() failed!");
        sz_total_read += sz_read;
        if (sz_total_read == sz)
            return;
    }
}

char* rm_comments(char* orig, ssize_t origsz) {
    char* read_cursor;
    char* write_cursor;
    size_t len;
    size_t offset;
    char* result;

    result = (char*)overload_malloc(origsz);
    read_cursor = orig;
    write_cursor = result;
    offset = 0;
    while (1) {
        read_cursor = orig + offset;
        len = len_line(read_cursor, origsz - offset);
        if (len == -1)
            break;
        if (read_cursor[0] == '#')
            goto nextline;
        strncpy(write_cursor, read_cursor, len + 1);
        write_cursor += len + 1;
    nextline:
        offset += len + 1;
    }
    *write_cursor = '\0';

    return result;
}

ssize_t paragraph_len(char* str, ssize_t len) {
    ssize_t i;

    for (i = 0; i < len - 1; i++) {
        if (str[i] == '\n' && str[i + 1] == '\n')
            return i;
    }
    return -1;
}

size_t parse_regions(char* str, struct mregion** regions_ptr) {
    int i;
    struct mregion* regions;
    struct mregion* r;
    size_t nr_regions;
    char** lines;
    char** fields;
    int nr_fields;

    nr_regions = astr_split(str, '\n', &lines);
    if (nr_regions < 1)
        err(1, "Not enough lines");
    regions = (struct mregion*)overload_malloc(sizeof(struct mregion) * nr_regions);

    for (i = 0; i < nr_regions; i++) {
        r = &regions[i];
        nr_fields = astr_split(lines[i], ',', &fields);
        if (nr_fields != 2)
            err(1, "Wrong format config file: %s", lines[i]);
        strcpy(r->name, fields[0]);
        r->sz = atoll(fields[1]);
        astr_free_str_array(fields, nr_fields);
    }

    astr_free_str_array(lines, nr_regions);
    *regions_ptr = regions;

    return nr_regions;
}

enum rw_mode parse_rwmode(char* input) {
    char* rwmode = overload_malloc(strlen(input) + 1);

    sscanf(input, "%s", rwmode);

    if (!strncmp(rwmode, "ro", 2)) {
        return READ_ONLY;
    }
    else if (!strncmp(rwmode, "wo", 2)) {
        return WRITE_ONLY;
    }
    else if (!strncmp(rwmode, "rw", 2)) {
        return READ_WRITE;
    }
    else {
        fprintf(stderr, "wrong rw mode: %s\n", rwmode);
        exit(1);
    }
}

/**
 * parse_phase - Parse a phase from string lines
 *
 * Return number of lines for this phase
 */
int parse_phase(char* lines[], int nr_lines, struct phase* p,
    size_t nr_regions, struct mregion* regions) {
    struct access* patterns;
    char** fields;
    int nr_fields;
    struct access* a;
    int j, k;

    if (nr_lines < 3) {
        //print lines
        for (int i = 0; i < nr_lines; i++) {
            printf("%s\n", lines[i]);
        }
        errx(1, "%s: Wrong number of lines! %d\n", __func__, nr_lines);
    }

    p->name = (char*)overload_malloc((strlen(lines[0]) + 1) * sizeof(char));
    strcpy(p->name, lines[0]);
    p->time_ms = atoi(lines[1]);
    p->nr_patterns = nr_lines - 2;
    p->total_probability = 0;
    lines += 2;
    patterns = (struct access*)overload_malloc(p->nr_patterns *
        sizeof(struct access));
    p->patterns = patterns;
    for (j = 0; j < p->nr_patterns; j++) {
        nr_fields = astr_split(lines[0], ',', &fields);
        if (nr_fields != 4 && nr_fields != 5)
            err(1, "Wrong number of fields! %s\n",
                lines[0]);
        a = &patterns[j];
        a->mregion = NULL;
        for (k = 0; k < nr_regions; k++) {
            if (strcmp(fields[0], regions[k].name) == 0) {
                a->mregion = &regions[k];
            }
        }
        if (a->mregion == NULL)
            err(1, "Cannot find region with name %s",
                fields[0]);
        a->random_access = atoi(fields[1]);
        a->stride = atoi(fields[2]);
        a->probability = atoi(fields[3]);
        if (nr_fields == 5) {
            a->rw_mode = parse_rwmode(fields[4]);
        }
        else {
            a->rw_mode = default_rw_mode;
        }
        // printf("Using the mode %d\n",a->rw_mode);
        a->prob_start = p->total_probability;
        a->last_offset = 0;
        a->last_page = 0;
        lines++;
        astr_free_str_array(fields, nr_fields);
        p->total_probability += a->probability;
    }
    return 2 + p->nr_patterns;
}

size_t parse_phases(char* str, struct phase** phases_ptr,
    size_t nr_regions, struct mregion* regions) {
    struct phase* phases;
    struct phase* p;
    size_t nr_phases;
    char** lines_orig;
    char** lines;
    int nr_lines;
    int nr_lines_paragraph;
    int i, j;

    nr_phases = 0;
    nr_lines = astr_split(str, '\n', &lines_orig);
    lines = lines_orig;
    if (nr_lines < 4) /* phase name, time, nr patterns, pattern */
        err(1, "Not enough lines for phases %s", str);

    for (i = 0; i < nr_lines; i++) {
        if (lines_orig[i][0] == '\0')
            nr_phases++;
    }

    phases = (struct phase*)overload_malloc(nr_phases * sizeof(struct phase));

    for (i = 0; i < nr_phases; i++) {
        nr_lines_paragraph = 0;
        for (j = 0; j < nr_lines; j++) {
            if (lines[j][0] == '\0')
                break;
            nr_lines_paragraph++;
        }

        p = &phases[i];
        lines += parse_phase(&lines[0], nr_lines_paragraph,
            p, nr_regions, regions) +
            1;
    }
    astr_free_str_array(lines_orig, nr_lines);

    *phases_ptr = phases;
    return nr_phases;
}

void read_config(char* cfgpath, struct access_config* config_ptr) {
    struct stat sb;
    char* cfgstr;
    int f;
    char* content;
    int len_paragraph;
    size_t nr_regions;
    struct mregion* mregions;

    size_t nr_phases;
    struct phase* phases;

    f = open(cfgpath, O_RDONLY);
    if (f == -1)
        err(1, "open(\"%s\") failed", cfgpath);
    if (fstat(f, &sb))
        err(1, "fstat() for config file (%s) failed", cfgpath);
    cfgstr = (char*)overload_malloc(sb.st_size * sizeof(char));
    readall(f, cfgstr, sb.st_size);
    close(f);

    content = rm_comments(cfgstr, sb.st_size);
    free(cfgstr);

    len_paragraph = paragraph_len(content, strlen(content));
    if (len_paragraph == -1)
        err(1, "Wrong file format");
    content[len_paragraph] = '\0';
    nr_regions = parse_regions(content, &mregions);

    content += len_paragraph + 2; /* plus 2 for '\n\n' */
    nr_phases = parse_phases(content, &phases, nr_regions, mregions);

    config_ptr->regions = mregions;
    config_ptr->nr_regions = nr_regions;
    config_ptr->phases = phases;
    config_ptr->nr_phases = nr_phases;
}

static struct argp_option options[] = {
    {
        .name = "pr_config",
        .key = 'p',
        .arg = 0,
        .flags = 0,
        .doc = "flag for configuration content print",
        .group = 0,
    },
     {
        .name = "asm_skip",
        .key = 's',
        .arg = 0,
        .flags = 0,
        .doc = "skip asm ",
        .group = 0,
    },
    {
        .name = "dry_run",
        .key = 'd',
        .arg = 0,
        .flags = 0,
        .doc = "flag for dry run; If this flag is set, "
               "the program ends without real access",
        .group = 0,
    },
    {
        .name = "quiet",
        .key = 'q',
        .arg = 0,
        .flags = 0,
        .doc = "suppress all normal output",
        .group = 0,
    },
    {
        .name = "silent",
        .key = 'q',
        .arg = 0,
        .flags = OPTION_ALIAS,
        .doc = "suppress all normal output",
        .group = 0,
    },
    {
        .name = "hint",
        .key = 't',
        .arg = "<hint method>",
        .flags = 0,
        .doc = "gives access pattern hint to system with given method",
        .group = 0,
    },
    {
        .name = "default_rw_mode",
        .key = 'r',
        .arg = "<ro|wo|rw>",
        .flags = 0,
        .doc = "set default read/write mode as this",
        .group = 0,
    },
    {} };

char* config_file = "configs/default";
int do_print_config;
int dryrun;

error_t parse_option(int key, char* arg, struct argp_state* state) {
    switch (key) {
    case ARGP_KEY_ARG:
        if (state->arg_num > 0)
            argp_usage(state);
        config_file = (char*)overload_malloc((strlen(arg) + 1) * sizeof(char));
        strcpy(config_file, arg);
        break;
    case 'p':
        do_print_config = 1;
        break;
    case 'd':
        dryrun = 1;
        break;
    case 'q':
        quiet = 1;
        break;
    case 's':
        skip_asm = 1;
        break;
    case 't':
        if (strcmp("madvise", arg) == 0) {
            hintmethod = MADVISE;
            break;
        }
        if (strcmp("mlock", arg) == 0) {
            hintmethod = MLOCK;
            break;
        }
        fprintf(stderr, "hint should be madvise or mlock, not %s\n",
            arg);
        return ARGP_ERR_UNKNOWN;
    case 'r':
        if (!strcmp("ro", arg)) {
            default_rw_mode = READ_ONLY;
            break;
        }
        else if (!strcmp("wo", arg)) {
            default_rw_mode = WRITE_ONLY;
            break;
        }
        else if (!strcmp("rw", arg)) {
            default_rw_mode = READ_WRITE;
            break;
        }
        fprintf(stderr, "wrong default_rwmode input %s\n", arg);
        return ARGP_ERR_UNKNOWN;
    default:
        return ARGP_ERR_UNKNOWN;
    }

    return 0;
}

int tot_exec = 1;
void signal_handler(int signal_num) {
    printf("Caught signale %d\n", signal_num);
    if (signal_num == SIGINT) {
        if (tot_exec == 0)
            tot_exec++;
        else
            exit(signal_num);
    }

    if ((signal_num == SIGTSTP) & (tot_exec == 0))
        exit(signal_num);
    // It terminates the  program
    // exit(signal_num);
}

#define DONE_FILE "/tmp/done"
// The function creates a new file in /tmp directory to indicate completion
// use shell command touch to do so
void flag_done() {
    char cmd[200];
    sprintf(cmd, "touch %s\n", DONE_FILE);
    system(cmd);
}


int main(int argc, char* argv[]) {

    struct timeval fstop, fstart;
    struct access_config config;
    struct argp argp = {
        .options = options,
        .parser = parse_option,
        .args_doc = "[config file]",
        .doc =
            "Simulate given memory access pattern\v"
            "\'config file\' argument is optional."
            "  It defaults to \'configs/default\'",
    };
    argp_parse(&argp, argc, argv, ARGP_IN_ORDER, NULL, NULL);
    setlocale(LC_NUMERIC, "");

    fprintf(stderr, "\n*************\n");

    fprintf(stderr, "\n*************\n");

    signal(SIGINT, signal_handler);
    signal(SIGUSR1, signal_handler);
    signal(SIGUSR2, signal_handler);
    signal(SIGTSTP, signal_handler);

    printf("pid is %d\n",getpid());

    // if AUTO_MODE is defined print it else print its not defined
#ifdef AUTO_MODE
    fprintf(stderr, "AUTO_MODE is defined\n");
#else
    fprintf(stderr, "AUTO_MODE is not defined. After the allocation the program will wait for SIGTSTP signal.\n");
#endif
#ifdef PIN_MODE
    fprintf(stderr, "PIN_MODE is defined. Ensure it is running within PIN.\n");
#else
    fprintf(stderr, "PIN_MODE is not defined\n");
#endif
#ifdef OPS_MODE
    fprintf(stderr, "OPS_MODE is defined. \n");
#else
    fprintf(stderr, "OPS_MODE is not defined\n");
#endif

    usleep(1000000);






    read_config(config_file, &config);
    if (do_print_config && !quiet) {
        pr_regions(config.regions, config.nr_regions);

        pr_phases(config.phases, config.nr_phases);
    }

    if (dryrun)
        return 0;

    init_rndints();

    init_memory_regions(&config);




#ifdef PIN_MODE
    if (skip_asm == 0) {
        asm volatile ("int $100");
    }
#else
#ifndef AUTO_MODE
    sigset_t signal_set;
    int sig_number;
    fprintf(stderr, "Waiting for SIGTSTP signal\n");
    if (sigaddset(&signal_set, SIGTSTP) == -1)
        exit(1);
    sigwait(&signal_set, &sig_number);
#endif
#endif

int nr_iters=1;

    gettimeofday(&fstart, NULL);
    for(int n=0;n<nr_iters;n++)
    {
        fprintf(stderr, "Iteration %d/%d\n", n+1, nr_iters);
        exec_config(&config);
    }
    gettimeofday(&fstop, NULL);


    // print total time in seconds milliseconds and microseconds
    fprintf(stderr, "TOTAL_TIME_SEC %f\n", (fstop.tv_sec - fstart.tv_sec) + (fstop.tv_usec - fstart.tv_usec) / 1000000.0);
    fprintf(stderr, "TOTAL_TIME_MS %f\n", (fstop.tv_sec - fstart.tv_sec) * 1000 + (fstop.tv_usec - fstart.tv_usec) / 1000.0);
    fprintf(stderr, "TOTAL_TIME %ld\n", (fstop.tv_sec - fstart.tv_sec) * 1000000 + fstop.tv_usec - fstart.tv_usec);



    //	getchar();
    return 0;
}
