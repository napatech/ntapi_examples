#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <nt.h>
#include <stdarg.h>
#include <time.h>
#include <unistd.h>

#include "common.h"
#include "mpmc_queue.h"
#include "rx.h"
#include "flowlearn.h"
#include "flowtable.h"
#include "signal.h"
#include "stats.h"

#define QUEUE_SIZE (1 << 24)
#define POLL_THROTTLE_MASK 0xffffffff

static void *fl_poll_main(void *arg);
static void *fl_program_main(void *arg);
static void *fl_record_main(void *arg);
static void *fl_status_main(void *arg);

enum {
  THREAD_FLOW_POLL,
  THREAD_FLOW_PROGRAM,
  THREAD_FLOW_RECORD,
  THREAD_FLOW_STATUS,
  THREAD_FLOW_COUNT
};

static struct {
  void *(* main)(void *arg);
  pthread_t handle;
} thread[THREAD_FLOW_COUNT] = {
  { fl_poll_main },
  { fl_program_main },
  { fl_record_main },
  { fl_status_main }
};

static struct mpmc_queue program_queue;
static NtFlowStream_t flow_stream = NULL;

static unsigned poll_interval = 60;
static struct flow_entry *poll_head = NULL;
static pthread_mutex_t poll_mutex = PTHREAD_MUTEX_INITIALIZER;
static uint64_t poll_current = 0;
static unsigned poll_offset = 0;
static unsigned poll_op_count = 0;

static uint32_t fl_flags = 0;
static pid_t pid = 0;

/**
 * Although we pass the pointers towards the adapter, and they
 * get returned in the 64-bit ID; we attach some meta information
 * in the unused bits of the pointer using a bitfield.
 *
 * This is possible on almost all 64-bit architectures (with
 * the exception of PowerPC currently).  The AMD64 architecture
 * does allow the full 64 bits to be used but currently it is
 * limited by the paging levels;  5-level paging will extend
 * the virtual address space to 57-bits.
 *
 * This code is robust up to 62 bits if the defines in common.h are
 * adjusted.  Otherwise, the best approach will be to implement
 * your own memory pool with a lower address space so that
 * meta information can be passed along.
 */

#define TAG_BITS    (sizeof(((struct NtFlow_s *)0)->id) * 8 - (VIRTUAL_MEM_BITS - 1))

#if defined(__STDC_VERSION__)
  #if __STDC_VERSION__ >= 201112
    static_assert(TAG_BITS >= 2);
  #endif
#endif

union tagged_flow {
  uint64_t value;
  struct {
    unsigned  tag     : TAG_BITS;
    uintptr_t pointer : (VIRTUAL_MEM_BITS - 1);
  };
};

#ifdef MEMDEBUG
static const char *fl_status_record_name(uint32_t flag)
{
  switch (flag) {
    case NT_FLOW_STAT_LDS: return "LDS";
    case NT_FLOW_STAT_LFS: return "LFS";
    case NT_FLOW_STAT_LIS: return "LIS";
    case NT_FLOW_STAT_UIS: return "UIS";
    case NT_FLOW_STAT_UDS: return "UDS";
#if defined(NT_FLOW_STAT_RDS) && defined(NT_FLOW_STAT_RIS)
    case NT_FLOW_STAT_RDS: return "RDS";
    case NT_FLOW_STAT_RIS: return "RIS";
#endif
#if defined(NT_FLOW_STAT_PDS) && defined(NT_FLOW_STAT_PIS)
    case NT_FLOW_STAT_PDS: return "PDS";
    case NT_FLOW_STAT_PIS: return "PIS";
#endif
    default: return "INVALID";
  }
}
#endif

static void fl_remove_poll(struct flow_entry *entry)
{
  /* remove flow from probe list */
  pthread_mutex_lock(&poll_mutex);
  if (poll_head == entry) {
    if (entry->poll.next == entry)
      poll_head = NULL;
    else {
      poll_head = entry->poll.next;
    }
  }
  entry->poll.prev->poll.next = entry->poll.next;
  entry->poll.next->poll.prev = entry->poll.prev;
  ++poll_op_count;
  pthread_mutex_unlock(&poll_mutex);
}

static void fl_add_poll(struct flow_entry *entry)
{
  pthread_mutex_lock(&poll_mutex);

  assert(!entry->poll.next);
  assert(!entry->poll.prev);

  if (!poll_head) {
    poll_head = entry;
    entry->poll.next = entry;
    entry->poll.prev = entry;
  } else {
    entry->poll.next = poll_head;
    entry->poll.prev = poll_head->poll.prev;
    entry->poll.prev->poll.next = entry;
    poll_head->poll.prev = entry;
  }

  ++poll_op_count;
  if ((poll_op_count & POLL_THROTTLE_MASK) == 0) {
    ++poll_offset;
  }
  entry->poll.next_update = poll_current + poll_interval + poll_offset;
  pthread_mutex_unlock(&poll_mutex);
}

/* cheap operations must be done here otherwise there's a high chance that
 * statuses will drop */
static void *fl_status_main(void *arg)
{
  int ret;
  char buffer[NT_ERRBUF_SIZE];

  while (atomic_load_explicit(&app_running, memory_order_relaxed)) {
    NtFlowStatus_t status;

    if ((ret = NT_FlowStatusRead(flow_stream, &status)) == NT_SUCCESS) {
      union tagged_flow flow_id = { .value = status.id };
      struct flow_entry *entry = (struct flow_entry *)(uintptr_t)flow_id.pointer;
      uint32_t flags, probe_flag;

#ifdef MEMDEBUG
      /* when debugging memory, we use the tag to store lower 16-bits of hash
       * instead to validate the memory of the entry */
      if (ft_validate(flow_id.tag, entry) != 0) {
        fprintf(stderr, "error: invalid pointer used in %s status record: %p\n",
                fl_status_record_name(status.flags), entry);
        continue;
      }
#else
      /* if there was an earlier instance of a flow program, some of the info
       * records can come from flows that we don't track; use the pid indicator
       * to filter those out */
      if (flow_id.tag != pid)
        continue;
#endif

      switch (status.flags) {
        case NT_FLOW_STAT_LIS: /* Learn ignore status flag */
#ifdef MEMDEBUG
          struct flow_entry *e;
          /* validate the flow */
          if ((e = ft_lookup(entry->hash, entry->key_data, entry->color)) != entry) {
            fprintf(stderr, "warning: two entries with same key %p and %p\n",
                    e, entry);
            break;
          }
#endif
          /* fix the state... by falling through??? */
          fprintf(stderr, "warning: learn ignored with flow %p\n", entry);
          break;
        case NT_FLOW_STAT_LDS: /* Learn done status flag */
          flags = ~(FLOW_FLAGS_SOFTWARE | FLOW_FLAGS_REFRESH | FLOW_FLAGS_REFRESH_PENDING | FLOW_FLAGS_LEARN_PENDING);
          flags = atomic_fetch_and_explicit(&entry->flags, flags, memory_order_relaxed);

          /* learn operation */
          if (flags & FLOW_FLAGS_SOFTWARE) {
            /* successfully learnt flow; TODO: add hook? */
          }
          if (flags & FLOW_FLAGS_REFRESH_PENDING)
            stats_update(STAT_REFRESH, 1);

          /* if it has already been handled here, we don't need to skip the next poll */
          atomic_fetch_sub_explicit(&entry->poll.skip,
              __builtin_popcount(flags & (FLOW_FLAGS_REFRESH_PENDING | FLOW_FLAGS_LEARN_PENDING)),
              memory_order_relaxed);
          break;
        case NT_FLOW_STAT_LFS: /* Learn fail status flag */
          fprintf(stderr, "warning: failed to learn flow %p\n", entry);
          flags = atomic_fetch_and_explicit(&entry->flags, ~(FLOW_FLAGS_TRACK | FLOW_FLAGS_REFRESH_PENDING | FLOW_FLAGS_LEARN_PENDING),
                                            memory_order_relaxed);
          /* if it has already been handled here, we don't need to skip the next poll */
          atomic_fetch_sub_explicit(&entry->poll.skip, 1, memory_order_relaxed);
          break;
        case NT_FLOW_STAT_UIS: /* Unlearn ignore status flag */
          fprintf(stderr, "warning: unlearn ignored with flow %p\n", entry);
          break;
        case NT_FLOW_STAT_UDS: /* Unlearn done status flag */
          probe_flag = (fl_flags & FL_FLAGS_HW_PROBE) ? 0 : FLOW_FLAGS_PROBE_PENDING;
          flags = atomic_fetch_and_explicit(&entry->flags,
                ~(FLOW_FLAGS_UNLEARN_PENDING | probe_flag),
                memory_order_relaxed);
          /* if it has already been handled here, we don't need to skip the next poll */
          atomic_fetch_sub_explicit(&entry->poll.skip, 1, memory_order_relaxed);
          break;
#if defined(NT_FLOW_STAT_RDS) && defined(NT_FLOW_STAT_RIS)
        case NT_FLOW_STAT_RDS: /* Relearn done status flag */
         break;
        case NT_FLOW_STAT_RIS: /* Relearn ignore status flag */
          break;
#endif
#if defined(NT_FLOW_STAT_PDS) && defined(NT_FLOW_STAT_PIS)
        case NT_FLOW_STAT_PDS: /* Probe done status flag */
        case NT_FLOW_STAT_PIS: /* Probe ignore status flag */
#if 0
          /* remove probe flag so that the probe thread can send it next iteration */
          flags = atomic_fetch_and_explicit(&entry->flags, ~(FLOW_FLAGS_PROBE),
                                            memory_order_relaxed);
#endif
          if (status.flags == NT_FLOW_STAT_PIS)
            fprintf(stderr, "warning: state inconsistency detected with flow %p:"
                            " flow probe has been ignored\n", entry);
#if 0
          if (!(flags & FLOW_FLAGS_PROBE))
            fprintf(stderr, "warning: state inconsistency detected with flow %p:"
                            " flow isn't marked as probe\n", entry);
#endif
          break;
#endif
        default:
          fprintf(stderr, "NT_FlowStatusRead(): unexpected status flag: 0x%04x\n",
                  status.flags);
      }
    } else if (ret == NT_STATUS_TRYAGAIN) {
      usleep(1);
    } else {
      NT_ExplainError(ret, buffer, sizeof(buffer));
      fprintf(stderr, "error: NT_FlowStatusRead() failed: %s\n", buffer);
    }
  }
  return NULL;
}

static void fl_flow(struct NtFlow_s *flow, const struct flow_entry *entry, int op)
{
#ifdef MEMDEBUG
  union tagged_flow flow_id = { .tag = entry->hash, .pointer = (uintptr_t)entry };
#else
  union tagged_flow flow_id = { .tag = pid, .pointer = (uintptr_t)entry };
#endif
  memset(flow, 0, sizeof(struct NtFlow_s));
  //flow->color = entry->color | FLOW_COLOR_MONITOR;
  //flow->color = 32;
  //flow->color = entry->color;
  flow->color = 0;
  flow->ipProtocolField = fk_proto(entry->color);
  flow->keyId = fk_key_id(entry->color);
  /* TODO: flags is atomic and we need to integrate this with RELEARN support */
  //flow->keySetId = fk_key_id(entry->color);
  flow->keySetId = FLOW_KEYSET_ID_BYPASS;
  flow->op = op;
  flow->tau = (entry->color & FLOW_COLOR_TCP) ? 1 : 0;
  flow->gfi = 1;
  flow->id = flow_id.value;
  memcpy(flow->keyData, entry->key_data, (entry->color & FLOW_COLOR_IPV6) ? FLOW_IPV6_KEY_LEN : FLOW_IPV4_KEY_LEN);
}

static void *fl_record_main(void *arg)
{
  int status;
  char buffer[NT_ERRBUF_SIZE];

  if (fl_flags & FL_FLAGS_VERBOSE)
    printf("Time             | Src             - Dest            | Protocol         | Src port | Dest port | TCP flags | Bytes\n");

  while (atomic_load_explicit(&app_running, memory_order_relaxed)) {
    NtFlowInfo_t info;

    if ((status = NT_FlowRead(flow_stream, &info, 1000)) == NT_SUCCESS) {
      union tagged_flow flow_id = { .value = info.id };
      struct flow_entry *entry = (struct flow_entry *)(uintptr_t)flow_id.pointer;
      bool sw_probe = !(fl_flags & FL_FLAGS_HW_PROBE) && (info.cause == 0);

#ifdef MEMDEBUG
      /* when debugging memory, we use the tag to store lower 16-bits of hash
       * instead to validate the memory of the entry */
      if (ft_validate(flow_id.tag, entry) != 0) {
        fprintf(stderr, "error: invalid pointer used in info record: %p\n", entry);
        continue;
      }
#else
      /* if there was an earlier instance of a flow program, some of the info records
       * can come from flows that we don't track; use the pid indicator to filter those out */
      if (flow_id.tag != pid)
        continue;
#endif

      struct flow_info *fi = &entry->info;
      unsigned flags = atomic_fetch_or_explicit(
        &entry->flags, FLOW_FLAGS_SOFTWARE,
        memory_order_relaxed);
      bool unlearn = (flags & FLOW_FLAGS_UNLEARN);
      bool changed = false;

      /* true if software probe is currently active in this flow info record */
      sw_probe = sw_probe && (flags & FLOW_FLAGS_PROBE);


      /* update the statistics for the flow using the flow info record */
      changed = fi_update(fi, &info);

      /* if hw probing is supported; we don't need to do more in the normal
       * case because the flow is still learnt and doesn't require relearning.
       * However, if there are no packets flowing, we might have to forcefully
       * unlearn otherwise we will have to wait for the flow scrubber to time
       * out. */
      switch (info.cause) {
        case 0:
          /* learn again if software caused it (potentially software probing (without hw support)),
           * unless there were no stats changes; and then we remove the flow
           * entirely from the sw flow table (after this if statement).  Also if there
           * is an unlearn pending, we just let this carry through to the end
           * so that it performs the unlearning behaviour */
          if (sw_probe) {
            stats_update(STAT_PROBE, 1);
            //if (changed && !unlearn) {
            if (!unlearn) {
              atomic_fetch_and_explicit(&entry->flags, ~FLOW_FLAGS_PROBE_PENDING, memory_order_relaxed);
              fl_program(entry, FL_OP_REFRESH);
              continue;
            }
          }
          /* TODO: we need to issue a relearn if we didn't receive any more packets */
          break;
        case 4:
          stats_update(STAT_PROBE, 1);

          /* unlearn when there's no activity on the flow */
          if (!changed)
            fl_program(entry, FL_OP_UNLEARN);

          /* do not deref when there's a HW PROBE */
          continue;
      }

      /* no more probing needed */
      atomic_fetch_and_explicit(&entry->flags, ~FLOW_FLAGS_TRACK, memory_order_relaxed);
    } else if (status != NT_STATUS_TIMEOUT) {
      NT_ExplainError(status, buffer, sizeof(buffer));
      fprintf(stderr, "error: NT_FlowRead() failed: %s\n", buffer);
    }
  }

  return NULL;
}

static void *fl_poll_main(void *arg)
{
  struct timespec sleep_time;
  bool sw_probe = !(fl_flags & FL_FLAGS_HW_PROBE);
  uint32_t sw_probe_pending_flag = (sw_probe ? FLOW_FLAGS_PROBE_PENDING : 0);

  clock_gettime(CLOCK_MONOTONIC, &sleep_time);

  while (atomic_load_explicit(&app_running, memory_order_relaxed)) {
    ++sleep_time.tv_sec;

    pthread_mutex_lock(&poll_mutex);
    ++poll_current;
    poll_op_count = 0;
    pthread_mutex_unlock(&poll_mutex);

    while (1) {
      struct flow_entry *entry;
      uint32_t flags;

      /* traverse to the next element the linked list of probes */
      pthread_mutex_lock(&poll_mutex);
      if (!poll_head || poll_current < poll_head->poll.next_update) {
        pthread_mutex_unlock(&poll_mutex);
        break;
      }
      poll_head->poll.next_update = poll_current + poll_interval;
      entry = poll_head = poll_head->poll.next;
      pthread_mutex_unlock(&poll_mutex);

      /* check if we need to skip this poll because it is too close to
       * some calls made to the flow API, so we don't need to retry yet */
      if (atomic_exchange_explicit(&entry->poll.skip, 0, memory_order_relaxed) > 0)
        continue;

      /* check the status of the entry; we may need to retry */
      flags = atomic_fetch_and_explicit(&entry->flags, FLOW_FLAGS_CLEAR,
                                        memory_order_relaxed);

      /* probe the flow */
      if (flags & FLOW_FLAGS_LEARN_PENDING)
        fl_program(entry, FL_OP_LEARN);
      else if (flags & (FLOW_FLAGS_REFRESH_PENDING | sw_probe_pending_flag))
        fl_program(entry, FL_OP_REFRESH);
      else if (flags & FLOW_FLAGS_UNLEARN_PENDING)
        fl_program(entry, FL_OP_UNLEARN);
      else if ((flags & FLOW_FLAGS_UNLEARN) || !(flags & FLOW_FLAGS_TRACK)) {
        //fprintf(stderr, "untracked poll %p\n", entry);
        fl_remove_poll(entry);
        ft_deref(entry, 1); /* matches the original add_probe */
      } else if (flags & (FLOW_FLAGS_LEARN |
                          FLOW_FLAGS_PROBE |
                          FLOW_FLAGS_REFRESH |
                          FLOW_FLAGS_PROBE_PENDING)) {
        fl_program(entry, FL_OP_PROBE);
        //fprintf(stderr, "%p: %u\n", entry, flags);
      }
    }

    clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &sleep_time, NULL);
  }

  return NULL;
}

static void fl_print(const struct flow_entry *entry)
{
  const uint8_t *kd = entry->key_data;
  char ip4[2][16];
  uint16_t port[2] = {
    (((unsigned)kd[8]) << 8) | kd[9],
    (((unsigned)kd[10]) << 8) | kd[11]
  };
  const struct flow_info *fi = &entry->info;

  /* print out the flow */

  /* TODO: implement ipv6 handling */
  snprintf(ip4[0], sizeof(ip4[0]), "%d.%d.%d.%d",
    kd[0], kd[1], kd[2], kd[3]);
  snprintf(ip4[1], sizeof(ip4[1]), "%d.%d.%d.%d",
    kd[4], kd[5], kd[6], kd[7]);

  for (int i = 0; i < 2; ++i) {
    if (!fi->stream[i].packets)
      continue;
    printf("%016lx | %-15s - %-15s | %16s | %8u | %9u | %9u | %16lu\n",
      fi->ts, ip4[i], ip4[!i],
      (entry->color & FLOW_COLOR_TCP) ? "TCP" : "UDP",
      port[i], port[!i], fi->stream[i].flags, fi->stream[i].octets);
  }
}

static void *fl_program_main(void *arg)
{
  int status;
  char buffer[NT_ERRBUF_SIZE];
  uint64_t data;

  while (mpmc_queue_pull_wait(&program_queue, &data, &app_running)) {
    union tagged_flow f = { .value = data };
    struct flow_entry *entry = (struct flow_entry *)(uintptr_t)f.pointer;
    struct NtFlow_s flow;
    bool probe = false;
    bool sw_probe = !(fl_flags & FL_FLAGS_HW_PROBE);
    uint32_t sw_probe_flag = (sw_probe ? FLOW_FLAGS_PROBE : 0);
    unsigned op;
    uint32_t flags;

    mpmc_queue_wake_full(&program_queue);
    stats_update(STAT_QUEUE, -1);

    /* set the associated flag and skip if there is one already pending:
     * we don't want to reissue the same command twice otherwise this
     * complicates having to handle a queue of asynchronous calls which
     * do not give us any extra information *except* in the case of
     * relearning. */
    switch (f.tag) {
      case FL_OP_PROBE:
        flags = atomic_fetch_or_explicit(&entry->flags,
                                         FLOW_FLAGS_PROBE | FLOW_FLAGS_PROBE_PENDING | FLOW_FLAGS_TRACK,
                                         memory_order_relaxed);
#if 0
        if (sw_probe && (flags & FLOW_FLAGS_REFRESH)) {
          /* reissue a learn instead (it was probably handled in software in
           * the meantime) */
          op = 1;
          break;
        }
#endif
        probe = true;
        op = sw_probe ? 0 : 3;
        break;
      //case FL_OP_RELEARN:
      //  if (atomic_fetch_or(&entry->flags, FLOW_FLAGS_RELEARN) & FLOW_FLAGS_RELEARN) {
      //    /* TODO */
      //  }
      //  op = 2;
      //  break;
      case FL_OP_UNLEARN:
        flags = atomic_fetch_or_explicit(&entry->flags,
                                         FLOW_FLAGS_UNLEARN | FLOW_FLAGS_UNLEARN_PENDING,
                                         memory_order_relaxed);
        /* we can skip sending an unlearn command if we know that a probe command
         * is pending because an unlearn will automatically occur when it is using
         * the software emulated probe */
        if (flags & (FLOW_FLAGS_UNLEARN | sw_probe_flag))
          continue;
        op = 0;
        break;
      case FL_OP_REFRESH:
        flags = (atomic_fetch_or_explicit(&entry->flags,
                                          FLOW_FLAGS_REFRESH | FLOW_FLAGS_REFRESH_PENDING | FLOW_FLAGS_TRACK,
                                          memory_order_relaxed));
        if (flags & FLOW_FLAGS_REFRESH) {
          fprintf(stderr, "warning: redundant refresh for %p\n", entry);
          continue;
        }
        op = 1;
        break;
      case FL_OP_LEARN:
        flags = atomic_fetch_or_explicit(&entry->flags,
                                         FLOW_FLAGS_LEARN | FLOW_FLAGS_LEARN_PENDING | FLOW_FLAGS_TRACK,
                                         memory_order_relaxed);
        if (flags & FLOW_FLAGS_LEARN)
          continue;

        op = 1;

        if (!(flags & FLOW_FLAGS_TRACK)) {
          ft_ref(entry, 1);
          fl_add_poll(entry);
        }

        break;
      default:
        assert(0);
        continue;
    }

    fl_flow(&flow, entry, op);

    if (!probe && (fl_flags & FL_FLAGS_VERBOSE)) {
      fl_print(entry);
    }

    /* add a little time to the probing so that there's no race condition,
     * this is done by adding to a *skip* counter */
    if ((f.tag == FL_OP_LEARN) && (flags != FLOW_FLAGS_LEARN_PENDING))
      atomic_fetch_add_explicit(&entry->poll.skip, 1, memory_order_relaxed);

#ifdef LEARNDBG
    if (op == 1) {
      char src[16], dst[16];
      uint16_t srcport, dstport;
      srcport = (entry->key_data[8] << 8) | entry->key_data[9];
      dstport = (entry->key_data[10] << 8) | entry->key_data[11];
      snprintf(src, sizeof(src), "%d.%d.%d.%d", entry->key_data[0], entry->key_data[1], entry->key_data[2], entry->key_data[3]);
      snprintf(dst, sizeof(dst), "%d.%d.%d.%d", entry->key_data[4], entry->key_data[5], entry->key_data[6], entry->key_data[7]);
      fprintf(stderr, "learn: %p hash:%08x src:%s:%u dst:%s:%u color:%04x stream_id=%u\n", entry, entry->hash, src, srcport, dst, dstport, entry->color, entry->stream_id);
    }
#endif

    status = NT_FlowWrite(flow_stream, &flow, -1);
    if (status != 0) {
      NT_ExplainError(status, buffer, sizeof(buffer));
      fprintf(stderr, "error: NT_FlowWrite() failed: %s\n", buffer);
      break;
    }
  }

  return NULL;
}

int fl_init(unsigned adapter_no, unsigned interval, uint32_t flags)
{
  NtFlowAttr_t flow_attr;
  int status;
  char buffer[NT_ERRBUF_SIZE];

  fl_flags = flags;
  poll_interval = interval;
  pid = getpid();

  if ((status = mpmc_queue_init(&program_queue, QUEUE_SIZE)) < 0) {
    errno = -status;
    perror("mpmc_queue_init");
    goto init_failed;
  }

  NT_FlowOpenAttrInit(&flow_attr);
  NT_FlowOpenAttrSetAdapterNo(&flow_attr, adapter_no);

  status = NT_FlowOpen_Attr(&flow_stream, "ntnetflow", &flow_attr);
  if (status != NT_SUCCESS) {
    NT_ExplainError(status, buffer, sizeof(buffer));
    fprintf(stderr, "error: NT_FlowOpen_Attr() failed: %s\n", buffer);
    goto init_failed;
  }

  for (int i = 0; i < THREAD_FLOW_COUNT; ++i) {
    if (pthread_create(&thread[i].handle, NULL, thread[i].main, NULL) < 0)
      goto pthread_error;
  }

  return 0;

//deinit_flopen:
  NT_FlowClose(flow_stream);
init_failed:
  signal_shutdown();
  return -1;
pthread_error:
  NT_FlowClose(flow_stream);
  errno = -status;
  perror("pthread");
  signal_shutdown();
  return -1;
}

void fl_join(void)
{
  mpmc_queue_broadcast_empty(&program_queue);
  mpmc_queue_broadcast_full(&program_queue);

  for (int i = 0; i < THREAD_FLOW_COUNT; ++i)
    pthread_join(thread[i].handle, NULL);
}

void fl_deinit(void)
{
  struct flow_entry *start = poll_head;

  mpmc_queue_deinit(&program_queue);

  if (flow_stream) {
    char buffer[NT_ERRBUF_SIZE];
    int ret;
    int count = 0, info_count = 0, status_count = 0;
    if (poll_head) {
      struct NtFlow_s flow;
      do {
        fl_flow(&flow, poll_head, false);
        ret = NT_FlowWrite(flow_stream, &flow, -1);
        if (ret != NT_SUCCESS) {
          NT_ExplainError(ret, buffer, sizeof(buffer));
          fprintf(stderr, "error: NT_FlowWrite() failed: %s\n", buffer);
          break;
        }

        /* drain the status records */
        do {
          NtFlowStatus_t status;
          ret = NT_FlowStatusRead(flow_stream, &status);
          status_count += (ret == NT_SUCCESS);
        } while (ret == NT_SUCCESS);

        if (ret != NT_STATUS_TRYAGAIN) {
          NT_ExplainError(ret, buffer, sizeof(buffer));
          fprintf(stderr, "error: NT_FlowStatusRead() failed: %s\n", buffer);
          break;
        }

        ++count;
        poll_head = poll_head->poll.next;
      } while (start != poll_head);
    }

    /* drain the info records */
    do {
      NtFlowInfo_t info;
      ret = NT_FlowRead(flow_stream, &info, 100);
      info_count += (ret == NT_SUCCESS);
    } while (ret == NT_SUCCESS);

    if (ret != NT_STATUS_TIMEOUT) {
      NT_ExplainError(ret, buffer, sizeof(buffer));
      fprintf(stderr, "error: NT_FlowRead() failed: %s\n", buffer);
    }

    /* drain the status records */
    do {
      NtFlowStatus_t status;
      ret = NT_FlowStatusRead(flow_stream, &status);
      status_count += (ret == NT_SUCCESS);
    } while (ret == NT_SUCCESS);

    if (ret != NT_STATUS_TRYAGAIN) {
      NT_ExplainError(ret, buffer, sizeof(buffer));
      fprintf(stderr, "error: NT_FlowStatusRead() failed: %s\n", buffer);
    }

    printf("total probe list: %d drained info: %d drained status: %d\n", count, info_count, status_count);
    NT_FlowClose(flow_stream);
  }

  pthread_mutex_destroy(&poll_mutex);
}

int fl_program(const struct flow_entry *entry, unsigned op, ...)
{
  union tagged_flow f = {
    .tag = op,
    .pointer = (uintptr_t)entry
  };

  //if (op == FL_OP_RELEARN) {
  //  va_list args;
  //  va_start(args, op);
  //  f.stream = va_arg(args, unsigned);
  //  f.key_set = va_arg(args, unsigned);
  //  va_end(args);
  //}

  bool success = mpmc_queue_push_wait(&program_queue, f.value, &app_running);
  if (success) {
    mpmc_queue_wake_empty(&program_queue);
    stats_update(STAT_QUEUE, 1);
  }

#ifndef NDEBUG
  uint64_t sz = mpmc_queue_size(&program_queue);
  if (sz >= QUEUE_SIZE - 16) {
    fprintf(stderr, "queue size: %ld\n", sz);
  }
#endif

  return success;
}
