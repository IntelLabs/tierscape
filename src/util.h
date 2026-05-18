#pragma once

#include <cstdio>
#include <cstdarg>
#include <cstdint>
#include <sys/types.h>

// Logging
void log_set_verbose(bool v);
void log_info(const char* fmt, ...);
void log_verbose(const char* fmt, ...);
void log_warn(const char* fmt, ...);
void log_err(const char* fmt, ...);

// Process utilities
bool is_process_running(pid_t pid);
bool can_read_proc(pid_t pid);

// PID file
int write_pidfile(const char* path);
int remove_pidfile(const char* path);
