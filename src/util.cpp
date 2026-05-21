#include "util.h"

#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <signal.h>
#include <sys/stat.h>
#include <unistd.h>

static bool g_verbose = false;

void log_set_verbose(bool v) { g_verbose = v; }

static void log_prefix(FILE* fp, const char* level) {
    // ISO-8601 UTC timestamp.
    char ts[32];
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    struct tm tm_utc;
    gmtime_r(&now.tv_sec, &tm_utc);
    int n = std::strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", &tm_utc);
    std::snprintf(ts + n, sizeof(ts) - n, ".%03ldZ", now.tv_nsec / 1000000);
    std::fprintf(fp, "%s %s ", ts, level);
}

static void log_emit(const char* level, const char* fmt, va_list args) {
    log_prefix(stderr, level);
    std::vfprintf(stderr, fmt, args);
    std::fputc('\n', stderr);
}

void log_info(const char* fmt, ...) {
    va_list args; va_start(args, fmt); log_emit("[INFO]", fmt, args); va_end(args);
}
void log_warn(const char* fmt, ...) {
    va_list args; va_start(args, fmt); log_emit("[WARN]", fmt, args); va_end(args);
}
void log_err(const char* fmt, ...) {
    va_list args; va_start(args, fmt); log_emit("[ERR ]", fmt, args); va_end(args);
}
void log_verbose(const char* fmt, ...) {
    if (!g_verbose) return;
    va_list args; va_start(args, fmt); log_emit("[VERB]", fmt, args); va_end(args);
}

bool is_process_running(pid_t pid) {
    if (pid <= 0) return false;
    return kill(pid, 0) == 0;
}

bool can_read_proc(pid_t pid) {
    char path[64];
    std::snprintf(path, sizeof(path), "/proc/%d/maps", pid);
    return access(path, R_OK) == 0;
}

int write_pidfile(const char* path) {
    FILE* f = std::fopen(path, "w");
    if (!f) return -1;
    std::fprintf(f, "%d\n", getpid());
    std::fclose(f);
    return 0;
}

int remove_pidfile(const char* path) {
    return unlink(path);
}

bool parse_int(const char* s, long& out) {
    if (!s || !*s) return false;
    errno = 0;
    char* end = nullptr;
    long v = std::strtol(s, &end, 10);
    if (errno != 0 || end == s || (end && *end != '\0')) return false;
    out = v;
    return true;
}

bool parse_float(const char* s, double& out) {
    if (!s || !*s) return false;
    errno = 0;
    char* end = nullptr;
    double v = std::strtod(s, &end);
    if (errno != 0 || end == s || (end && *end != '\0')) return false;
    out = v;
    return true;
}
