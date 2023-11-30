#ifndef STATS_H
#define STATS_H

enum {
  STAT_LEARN,
  STAT_UNLEARN,
  STAT_REFRESH,
  STAT_PROBE,
  STAT_FLOW,
  STAT_QUEUE,
  STAT_END
};

int stats_init(int interval);
int stats_start(void);
int stats_stop(void);
void stats_deinit(void);
bool stats_enabled(void);

void stats_update(int entry, int64_t value);
int stats_snprintf(char *str, size_t sz, char line_ending);
void stats_fprintf(FILE *f, char line_ending);
int stats_csv_snprintf(char *str, size_t sz, char line_ending);
void stats_csv_fprintf(FILE *f, char line_ending);
#endif
