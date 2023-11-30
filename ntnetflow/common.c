#include "common.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

atomic_bool app_running = true;

int vmembits(void)
{
#ifdef __linux__
  FILE *f = fopen("/proc/cpuinfo", "r");
  size_t sz = 1024;
  char *line = malloc(sz);
  int ret = -1;

  if (f == NULL)
    return -1;

  while (!feof(f)) {
    ssize_t nb = getline(&line, &sz, f);

    if (nb < 0) {
      fprintf(stderr, "error: failed to read /proc/cpuinfo: %s\n",
              strerror(errno));
      goto free_line;
    }

    if (strstr(line, "address sizes")) {
      const char *p = strstr(line, "bits virtual");
      errno = 0;
      while (p > line) {
        if (*p == ',') {
          char *e;
          float v = strtof(p + 1, &e);
          if (e == p || errno)
            goto free_line;
          else {
            ret = (int)v;
            goto free_line;
          }
        }
        --p;
      }
    }
  }

free_line:
  free(line);
  fclose(f);

  return ret;
#endif
  return -1;
}
