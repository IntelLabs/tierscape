#ifndef UTILS_H
#define UTILS_H


#include <iostream>
#include <vector>
#include <algorithm>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <csignal>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <future>
#include <random>
#include <iostream>
#include <fstream>
#include <vector>
#include <sstream>


std::vector<int> readLastColumnIntegers(const std::string& filename);

#define MULTIPLIER_FACTOR 10
#define INIT_REGION_MB  2



// Macro definition
#define WARN_ONCE(message)                                           \
  static std::atomic<bool> warned_once_##__LINE__;                   \
  if (!warned_once_##__LINE__.exchange(true)) {                      \
    std::cerr << "WARNING: " << message << std::endl;                 \
  }

#define pr_info_highlight(...) \
    fprintf(stderr, "\n**********\n-------INFO: "); \
    fprintf(stderr, __VA_ARGS__); \
    fprintf(stderr, "*************\n");
    // fprintf(stdout, "%s:%s:%d: ", __FILE__, __FUNCTION__, __LINE__); 

#define pr_info(...) \
    fprintf(stderr, "-------INFO: "); \
    fprintf(stderr, __VA_ARGS__);
    // fprintf(stdout, "\n");
    // fprintf(stdout, "%s:%s:%d: ", __FILE__, __FUNCTION__, __LINE__); 

#define pr_warn(...) \
  fprintf(stderr, "-------WARN: " RESET); \
    fprintf(stderr, __VA_ARGS__);

#define pr_err(...) \
    fprintf(stderr, "==== ERROR at %s:%s:%d Msg:\n", __FILE__, __FUNCTION__, __LINE__); \
    fprintf(stderr, __VA_ARGS__); \
    // fprintf(stderr, "\n");


#endif