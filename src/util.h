#pragma once

#include <cstdint>
#include <sys/types.h>

// Logging
void log_set_verbose(bool v);
void log_info(const char* fmt, ...)    __attribute__((format(printf, 1, 2)));
void log_verbose(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void log_warn(const char* fmt, ...)    __attribute__((format(printf, 1, 2)));
void log_err(const char* fmt, ...)     __attribute__((format(printf, 1, 2)));

// Process utilities
bool is_process_running(pid_t pid);
bool can_read_proc(pid_t pid);

// PID file
int write_pidfile(const char* path);
int remove_pidfile(const char* path);

// Parse an integer from a C string. Returns true on full successful parse.
bool parse_int(const char* s, long& out);
bool parse_float(const char* s, double& out);
