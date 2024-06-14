#include <kernel.h>
#include <klib.h>
#include <klib-macros.h>

#undef INT_MIN
#define INT_MIN (-INT_MAX - 1)
#undef INT_MAX
#define INT_MAX __INT_MAX__
