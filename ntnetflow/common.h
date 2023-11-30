#ifndef NTNETFLOW_COMMON_H
#define NTNETFLOW_COMMON_H

#define NTPL_PFX       "NTNF"
#define PROGNAME       "ntnetflow"

#ifndef TOOLS_RELEASE_VERSION
#define TOOLS_RELEASE_VERSION "(demo application)"
#endif

#include <stdatomic.h>

extern atomic_bool app_running;

#define STR(a) #a
#define XSTR(a) STR(a)

#ifndef VIRTUAL_MEM_BITS
#if defined(__x86_64__)
# define VIRTUAL_MEM_BITS 48
//# define VIRTUAL_MEM_BITS 57
#elif defined(__aarch64__)
# define VIRTUAL_MEM_BITS 52
# ifdef MEMDEBUG
#  error "Unsupported architecture"
# endif
#else
# error "Unsupported architecture"
#endif
#endif

#ifdef MEMDEBUG
# define FT_POWER_LB 2
# define FT_POWER_UB (64 - (VIRTUAL_MEM_BITS - 1))
#else
# define FT_POWER_LB 4
# define FT_POWER_UB (VIRTUAL_MEM_BITS - 1)
#endif

int vmembits(void);

#endif
