#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <nt.h>

#include "common.h"
#include "rx.h"
#include "flowlearn.h"
#include "flowtable.h"
#include "signal.h"
#include "stats.h"

static pthread_t rx_thread[128];
static unsigned rx_stream_count = 0;

struct rx_stream {
  unsigned id;
  NtNetStreamRx_t handle;
} rx_stream[128] = {0};

static void rx_process(unsigned stream_id, const NtNetBuf_t netbuf)
{
  const NtDyn4Descr_t *dyn4 = NT_NET_GET_PKT_DESCR_PTR_DYN4(netbuf);
  enum NtTimestampType_e ts_type = NT_NET_GET_PKT_TIMESTAMP_TYPE(netbuf);
  uint32_t color = dyn4->color1 & UINT_MAX;
  uint8_t key_data[FLOW_MAX_KEY_LEN] = {0};
  const uint8_t *packet = (const uint8_t *)dyn4 + dyn4->descrLength;
  const uint8_t *l3 = packet + dyn4->offset0;
  const uint8_t *l4 = packet + dyn4->offset1;
  struct flow_entry *entry;
  uint64_t ts = dyn4->timestamp;

  /* NYI: need to test how colours are propagated for known flows */
#if 0
  /* packet is being monitored */
  if (color & FLOW_COLOR_MONITOR) {

    return;
  }
#endif
  fk_create(key_data, l3, l4, color);

  if (color & (FLOW_COLOR_UNHANDLED | FLOW_COLOR_MISS)) {
    entry = ft_lookup(dyn4->color1 >> 32, key_data, color & USHRT_MAX);
    if (!entry) {
      entry = ft_add(dyn4->color1 >> 32, key_data, color & USHRT_MAX);
#ifdef LEARNDBG
      if (ft_lookup(dyn4->color1 >> 32, key_data, color & USHRT_MAX) != entry) {
        fprintf(stderr, "warning: cannot find entry just added: %p\n", entry);
      }
#endif
      /* record which stream_id this entry came for other checks */
      entry->stream_id = stream_id;
      if (color & FLOW_COLOR_MISS)
        fl_program(entry, FL_OP_LEARN);
    }

    assert(ts_type == NT_TIMESTAMP_TYPE_UNIX_NANOTIME);
    if (ts_type == NT_TIMESTAMP_TYPE_NATIVE_UNIX)
      ts *= 10;

    fi_increment(&entry->info, l4[13], ts, color & FLOW_COLOR_DOWNSTREAM);
  }
}

static void *rx_main(void *arg)
{
  int status;
  struct rx_stream *s = arg;
  NtNetBuf_t netbuf;
  char buffer[NT_ERRBUF_SIZE];

  while (atomic_load_explicit(&app_running, memory_order_relaxed)) {
    status = NT_NetRxGetNextPacket(s->handle, &netbuf, 1000);
    if (status == NT_STATUS_TIMEOUT || status == NT_STATUS_TRYAGAIN) {
      continue;
    } else if (status != 0) {
      NT_ExplainError(status, buffer, sizeof(buffer));
      fprintf(stderr, "error: NT_NetRxGetNextPacket() failed: %s\n", buffer);
      break;
    }
    rx_process(s->id, netbuf);
  }

  return NULL;
}

int rx_init(uint32_t stream[], unsigned stream_count)
{
  int status;
  char buffer[NT_ERRBUF_SIZE];

  for (int i = 0; i < stream_count; ++i) {
    if ((status = NT_NetRxOpen(&rx_stream[i].handle, "ntnetflow",
                               NT_NET_INTERFACE_PACKET, stream[i], -1)) != 0) {
      NT_ExplainError(status, buffer, sizeof(buffer));
      fprintf(stderr, "error: NT_NetRxOpen() failed: %s\n", buffer);
      goto deinit_rxopen;
    }
    rx_stream[i].id = stream[i];
    ++rx_stream_count;
  }

  for (int i = 0; i < stream_count; ++i) {
    if (pthread_create(rx_thread + i, NULL, rx_main, rx_stream + i) < 0)
      goto pthread_error;
  }

  return 0;

deinit_rxopen:
  for (int i = 0; i < rx_stream_count; ++i) {
    NT_NetRxClose(rx_stream[i].handle);
  }
  signal_shutdown();
  return -1;

pthread_error:
  errno = -status;
  perror("pthread");
  signal_shutdown();
  return -1;
}

void rx_join(void)
{
  for (int i = 0; i < rx_stream_count; ++i) {
    pthread_join(rx_thread[i], NULL);
  }
}

void rx_deinit(void)
{
  char buffer[NT_ERRBUF_SIZE];
  int status;

  for (int i = 0; i < rx_stream_count; ++i) {
    if ((status = NT_NetRxClose(rx_stream[i].handle)) != NT_SUCCESS) {
      NT_ExplainError(status, buffer, sizeof(buffer));
      fprintf(stderr, "error: NT_NetRxOpen() failed: %s\n", buffer);
    }
  }
}
