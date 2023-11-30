#ifndef FLOWTABLE_H
#define FLOWTABLE_H

#include <stdatomic.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <pthread.h>

#include "flowkey.h"
#include "flowinfo.h"
#include "flowlearn.h"

struct flow_bucket;

#define FLOW_INDEX_MASK       ((1 << 6) - 1)

/**
 * Flow state transitions:
 * SW -> SW,LEARN,PENDING -> HW
 * HW -> HW,PROBE,PENDING -> HW,REFRESH,PENDING -> HW
 * HW -> HW,UNLEARN,PENDING
 */

/* flow state flags */
#define FLOW_FLAGS_UNLEARN         (1 <<  0)
#define FLOW_FLAGS_LEARN           (1 <<  1)
#define FLOW_FLAGS_REFRESH         (1 <<  2)
#define FLOW_FLAGS_PROBE           (1 <<  3)
#define FLOW_FLAGS_TRACK           (1 <<  4)
#define FLOW_FLAGS_SOFTWARE        (1 <<  7) /* when unset, is HARDWARE */
#define FLOW_FLAGS_UNCHANGED       (1 <<  8) /* when unset, is changed */
#define FLOW_FLAGS_UNLEARN_PENDING (1 <<  8) /* waiting for status */
#define FLOW_FLAGS_LEARN_PENDING   (1 <<  9) /* waiting for status */
#define FLOW_FLAGS_REFRESH_PENDING (1 << 10) /* waiting for status */
#define FLOW_FLAGS_PROBE_PENDING   (1 << 11) /* waiting for status */

#define FLOW_FLAGS_PENDING    (\
  FLOW_FLAGS_UNLEARN_PENDING  |\
  FLOW_FLAGS_LEARN_PENDING    |\
  FLOW_FLAGS_REFRESH_PENDING  |\
  FLOW_FLAGS_PROBE_PENDING    )

#define FLOW_FLAGS_CLEAR     ~(\
  FLOW_FLAGS_UNLEARN          |\
  FLOW_FLAGS_LEARN            |\
  FLOW_FLAGS_REFRESH          |\
  FLOW_FLAGS_PROBE            |\
  FLOW_FLAGS_UNLEARN_PENDING  |\
  FLOW_FLAGS_LEARN_PENDING    |\
  FLOW_FLAGS_REFRESH_PENDING  |\
  FLOW_FLAGS_PROBE_PENDING    )

/* TODO: used by other parts of the program (NYI) */
#define FLOW_FLAGS_MONITOR    (1 << 6)

struct flow_entry {
  pthread_rwlock_t rwlock;
  struct flow_bucket *bucket;
  uint32_t hash;       /* hash calculated by the adapter */
  uint8_t stream_id;
  uint8_t index;       /* bucket index (only 6-bits used currently) */
  uint16_t color;      /* colour for identifying flow type */
  atomic_int rc;       /* reference count for releasing */
  atomic_uint flags;   /* flags for flow management */
  uint8_t key_data[FLOW_MAX_KEY_LEN];
  struct flow_info info;
  struct flow_poll_list poll;
} __attribute__((aligned(64)));

int ft_init(size_t power2, size_t chain_size, size_t bucket_size);
void ft_deinit();
struct flow_entry *ft_add(uint32_t hash, const uint8_t *key_data, uint16_t color);
void ft_remove(struct flow_entry *entry); /* don't use ft_remove directly, use ft_deref */
struct flow_entry *ft_lookup(uint32_t hash, const uint8_t *key_data, uint16_t color);
int ft_validate(uint32_t hash, const struct flow_entry *entry);
void ft_ref(struct flow_entry *entry, uint32_t count);
void ft_deref(struct flow_entry *entry, uint32_t count);

#endif
