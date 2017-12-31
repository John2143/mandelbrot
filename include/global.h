#pragma once

#include <cstdio>
#include <cmath>
#include <SDL2/SDL.h>
#include <cstring>
#include <thread>

#ifdef __MINGW32__
#    include <mingw.thread.h>
#    include <mingw.mutex.h>
#    include <mingw.condition_variable.h>
#endif
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <future>

typedef double floating;
constexpr size_t MAX_SUPERSAMPLES = 20;

#define MARK() printf("%i\n", __LINE__); fflush(stdout);
#define BOOL(b) ((b) ? "true" : "false")
