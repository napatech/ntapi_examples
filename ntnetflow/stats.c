#include <errno.h>
#include <stdatomic.h>
#include <nt.h>
#include <time.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>

#include "stats.h"

struct stats_entry {
  atomic_long count;
  int64_t last;
  int64_t rate;
};

struct stats_entry stats[STAT_END] = {0};
const char *names[STAT_END] = {
  "learn",
  "unlearn",
  "refresh",
  "probe",
  "flows",
  "queue"
};
static timer_t timer_id;

static bool stats_on = false;
static uint64_t stats_interval = 1000UL;
static atomic_ulong stats_count = 0;

/* this handler is called every second to update statistics which then can
 * be atomic read from any thread */
static void stats_timer_handler(int sig __attribute__((unused)))
{
  for (int i = 0; i < STAT_END; ++i) {
    uint64_t last = stats[i].last;
    stats[i].last = atomic_load_explicit(&stats[i].count, memory_order_relaxed);
    atomic_store_explicit(&stats[i].rate, stats[i].last - last,
                          memory_order_relaxed);
  }
  atomic_fetch_add_explicit(&stats_count, 1, memory_order_relaxed);
}

int stats_init(int interval)
{
  struct sigevent sev = {
    .sigev_notify = SIGEV_SIGNAL,
    .sigev_signo = SIGALRM,
    .sigev_value.sival_ptr = &timer_id
  };

  /* create signal handler for timer signal */
  struct sigaction sa = {
    .sa_handler = stats_timer_handler
  };
  sigemptyset(&sa.sa_mask);
  if (sigaction(SIGALRM, &sa, NULL) < 0)
    return -1;

  /* disable timer signal until we're ready */
  if (stats_stop())
    return -1;

  /* create the timer */
  if (timer_create(CLOCK_MONOTONIC, &sev, &timer_id) == -1)
    return -1;

  stats_interval = (uint64_t)interval;
  struct itimerspec its = {
    .it_value.tv_sec = (long)interval / 1000L,
  };

  its.it_interval.tv_sec = its.it_value.tv_sec;
  its.it_value.tv_nsec = (long)(interval - its.it_value.tv_sec * 1000) * 1000000L,
  its.it_interval.tv_nsec = its.it_value.tv_nsec;

  if (timer_settime(timer_id, 0, &its, NULL) < 0) {
    timer_delete(timer_id);
    return -1;
  }

  stats_on = true;

  return 0;
}

int stats_start()
{
  sigset_t mask;
  sigemptyset(&mask);
  sigaddset(&mask, SIGALRM);
  return sigprocmask(SIG_UNBLOCK, &mask, NULL);
}

void stats_deinit()
{
  stats_stop();
  timer_delete(timer_id);
}

int stats_stop()
{
  sigset_t mask;
  sigemptyset(&mask);
  sigaddset(&mask, SIGALRM);
  return sigprocmask(SIG_SETMASK, &mask, NULL);
}

void stats_update(int entry, int64_t value)
{
  atomic_fetch_add_explicit(&stats[entry].count, value, memory_order_relaxed);
}

int stats_snprintf(char *str, size_t sz, char line_ending)
{
  double interval = stats_interval;
  interval /= 1000.0;
  int ret = 0, total = 0;
  uint64_t flow;

  for (int i = 0; i < STAT_FLOW; ++i) {
    ret = snprintf(str, sz, "%s: %11.0f f/s | ",
                   names[i], ((double)stats[i].rate) / interval);
    if (ret < 0)
      return ret;
    if (ret >= sz)
      return total + ret;
    str += ret;
    sz -= ret;
    total += ret;
  }

  for (int i = STAT_FLOW; i < STAT_END; ++i) {
    flow = atomic_load_explicit(&stats[i].count,
                                     memory_order_relaxed);
    ret = snprintf(str, sz, "%s: %9lu flows %c ",
                   names[i], flow, (i == STAT_END - 1) ? line_ending : '|');
    if (ret < 0)
      return ret;
    if (ret >= sz)
      return total + ret;
    str += ret;
    sz -= ret;
    total += ret;
  }

  return total;
}

void stats_fprintf(FILE *f, char line_ending)
{
  char buffer[1024];
  stats_snprintf(buffer, sizeof(buffer), line_ending);
  fputs(buffer, f);
  fflush(f);
}

int stats_csv_snprintf(char *str, size_t sz, char line_ending)
{
  uint64_t ms = stats_interval * (atomic_load_explicit(&stats_count, memory_order_relaxed) - 1);
  int ret = 0, total = 0;
  uint64_t csv[STAT_END + 1], end = sizeof(csv)/sizeof(*csv);

  csv[0] = ms;
  for (int i = 1; i < STAT_END + 1; ++i)
    csv[i] = stats[i].count;

  for (int i = 0; i < end; ++i) {
    ret = snprintf(str, sz, "%lu%c", csv[i],
        (i == end - 1) ? line_ending : ' ');
    if (ret < 0)
      return ret;
    if (ret >= sz)
      return total + ret;
    str += ret;
    sz -= ret;
    total += ret;
  }

  return total;
}

void stats_csv_fprintf(FILE *f, char line_ending)
{
  char buffer[1024];
  stats_csv_snprintf(buffer, sizeof(buffer), line_ending);
  fputs(buffer, f);
  fflush(f);
}

bool stats_enabled(void)
{
  return stats_on;
}
