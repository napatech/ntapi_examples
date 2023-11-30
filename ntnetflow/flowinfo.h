#ifndef FLOWINFO_H
#define FLOWINFO_H

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>

struct NtFlowInfo_s;

struct flow_info_stream {
  uint64_t packets;
  uint64_t octets;
  uint16_t flags;
};

struct flow_info {
  pthread_mutex_t mutex;
  struct flow_info_stream stream[2];
  uint64_t ts, last_ts;
};

void fi_init(struct flow_info *info);
bool fi_update(struct flow_info *info, const struct NtFlowInfo_s *other);
bool fi_increment(struct flow_info *info, uint16_t flags, uint64_t ts, bool downstream);

#endif
