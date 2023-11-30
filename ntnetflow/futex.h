#ifndef NTNETFLOW_FUTEX_H
#define NTNETFLOW_FUTEX_H

#include <errno.h>
#include <linux/futex.h>
#include <stdint.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <unistd.h>

static inline ssize_t futex_wait(_Atomic uint32_t *addr, uint32_t val, atomic_bool *running)
{
  while (atomic_load_explicit(running, memory_order_relaxed)) {
    if (*addr != val)
      return 0;

    ssize_t ret = syscall(SYS_futex, addr, FUTEX_WAIT_PRIVATE, val, NULL, NULL, 0);
    if (ret < 0) {
      if (errno != EAGAIN)
        return -1;
    }
  }
  return -1;
}

static inline ssize_t futex_wake(_Atomic uint32_t *addr, int count)
{
  return syscall(SYS_futex, addr, FUTEX_WAKE_PRIVATE, count, NULL, NULL, 0);
}

#endif
