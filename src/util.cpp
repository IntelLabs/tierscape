#include "util.h"

#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>

static bool g_verbose = false;

void log_set_verbose(bool v) { g_verbose = v; }

void log_info(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "[INFO] ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
}

void log_verbose(const char* fmt, ...) {
    if (!g_verbose) return;
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "[VERB] ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
}

void log_warn(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "[WARN] ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
}

void log_err(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "[ERR]  ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
}

bool is_process_running(pid_t pid) {
    if (pid <= 0) return false;
    return (kill(pid, 0) == 0);
}

bool can_read_proc(pid_t pid) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/maps", pid);
    return (access(path, R_OK) == 0);
}

int write_pidfile(const char* path) {
    FILE* f = fopen(path, "w");
    if (!f) return -1;
    fprintf(f, "%d\n", getpid());
    fclose(f);
    return 0;
}

int remove_pidfile(const char* path) {
    return unlink(path);
}
