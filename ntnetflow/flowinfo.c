#include <stdlib.h>
#include <string.h>
#include <nt.h>
#include "flowinfo.h"

void fi_init(struct flow_info *info)
{
  memset(info, 0, sizeof(*info));
  pthread_mutex_init(&info->mutex, NULL);
}

static void fi_increment_stream(struct flow_info_stream *stream, uint64_t packets, uint64_t octets, uint16_t flags)
{
  stream->packets += packets;
  stream->octets += octets;
  stream->flags |= flags;
}

bool fi_update(struct flow_info *info, const struct NtFlowInfo_s *other)
{
  bool changed;

  pthread_mutex_lock(&info->mutex);
  fi_increment_stream(info->stream + 0, other->packetsA, other->octetsA, other->flagsA);
  fi_increment_stream(info->stream + 1, other->packetsB, other->octetsB, other->flagsB);
  changed = info->last_ts != other->ts;
  info->last_ts = info->ts;
  info->ts = other->ts;
  pthread_mutex_unlock(&info->mutex);

  return changed;
}

bool fi_increment(struct flow_info *info, uint16_t flags, uint64_t ts, bool downstream)
{
  bool changed;

  pthread_mutex_lock(&info->mutex);
  fi_increment_stream(info->stream + !!downstream, 1, 1, flags);
  changed = info->last_ts != ts;
  info->last_ts = info->ts;
  info->ts = ts;
  pthread_mutex_unlock(&info->mutex);

  return changed;
}
