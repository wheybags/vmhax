#pragma once

#include "../pinned.h"
#include <stdio.h>
#include <stdlib.h>

#ifdef _MSC_VER
#define DEBUG_BREAK() __debugbreak()
#else
#include <signal.h>
#define DEBUG_BREAK() raise(SIGTRAP)
#endif

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

#define CHECK(X) \
  do \
  { \
    if (!(X)) \
    { \
      fputs("CHECK FAILED: (" #X ") in " __FILE__ ":" TOSTRING(__LINE__) "\n", stderr); \
      DEBUG_BREAK(); \
      abort(); \
    } \
  } while(0)
