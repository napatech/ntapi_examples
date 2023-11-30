#include "common.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/eventfd.h>

#include "signal.h"

static int efd = -1;

/* called when user presses CTRL-C */
static void stop_app(int sig __attribute__((unused)))
{
  uint64_t number = 1;
  atomic_store_explicit(&app_running, false, memory_order_relaxed);
  write(efd, &number, sizeof(number));
}

/* initialise the signal handlers */
int signal_init(void)
{
  /* only initialise once otherwise error */
  if (efd != -1)
    return -1;

  struct sigaction new;
  memset(&new, 0, sizeof(new));
  new.sa_handler = stop_app;

  if (sigaction(SIGINT, &new, NULL) < 0) {
    fprintf(stderr, "error: failed to register SIGINT sigaction.\n");
    return -1;
  }
  if (sigaction(SIGTERM, &new, NULL) < 0) {
    fprintf(stderr, "error: failed to register SIGTERM sigaction.\n");
    return -1;
  }
  if (sigaction(SIGHUP, &new, NULL) < 0) {
    fprintf(stderr, "error: failed to register SIGHUP sigaction.\n");
    return -1;
  }

  /* return an event fd that should be monitored by any epoll */
  return (efd = eventfd(0, 0));
}

void signal_deinit(void)
{
  if (efd != -1)
    close(efd);
}

void signal_shutdown(void)
{
  stop_app(SIGTERM);
}
