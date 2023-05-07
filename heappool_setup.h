#pragma once
#include <signal.h>

#define VERBOSE(message...) log(message)

#define ALLOCATE_AT_LEAST_HALF

#define DOUBLE_FREE(x)   raise(SIGSEGV)
#define OUT_OF_MEMORY()  raise(SIGSEGV)

#define DISABLE_INTERRUPT()
#define ENABLE_INTERRUPT()
