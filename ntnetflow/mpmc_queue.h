/*
 * Copyright 2010-2011 Dmitry Vyukov
 * Copyright 2020 Patryk Stefanski
 *
 * Licensed under the Apache License, Version 2.0, <LICENSE-APACHE or
 * http://apache.org/licenses/LICENSE-2.0> or the MIT license <LICENSE-MIT or
 * http://opensource.org/licenses/MIT>, at your option. This file may not be
 * copied, modified, or distributed except according to those terms.
 *
 * Copyright 2023 Napatech A/S
 * Added wrappers using futexes.
 */

#ifndef BOUNDED_MPMC_QUEUE_H
#define BOUNDED_MPMC_QUEUE_H

#include <assert.h>
#include <errno.h>
#include <stdalign.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include "futex.h"

#define MAX_SPIN 4
#define CACHE_LINE_SIZE 64
#define likely(x)   __builtin_expect(!!(x),1)
#define unlikely(x) __builtin_expect(!!(x),0)

/*
 * Based on 'Bounded MPMC queue' by Dmitry Vyukov:
 * http://www.1024cores.net/home/lock-free-algorithms/queues/bounded-mpmc-queue
 */

struct mpmc_queue_cell {
  _Atomic uint32_t sequence;
  uint64_t data;
};

struct mpmc_queue {
  alignas(CACHE_LINE_SIZE) struct mpmc_queue_cell *buffer;
  uint32_t buffer_mask;
  alignas(CACHE_LINE_SIZE) atomic_uint head;
  alignas(CACHE_LINE_SIZE) atomic_uint tail;
  alignas(CACHE_LINE_SIZE) atomic_uint wait_empty;
  alignas(CACHE_LINE_SIZE) atomic_uint wait_full;
};

__attribute__((nonnull,warn_unused_result))
static inline int mpmc_queue_init(struct mpmc_queue *queue, uint32_t capacity)
{
  struct mpmc_queue_cell *buffer;

  assert(capacity >= 2 && (capacity & (capacity - 1)) == 0);

  if (posix_memalign((void **)&buffer, alignof(struct mpmc_queue), (size_t)capacity * sizeof(*buffer)))
    return -ENOMEM;

  for (uint32_t i = 0; i < capacity; i++)
    atomic_init(&buffer[i].sequence, i);

  queue->buffer = buffer;
  queue->buffer_mask = capacity - 1;
  atomic_init(&queue->head, 0);
  atomic_init(&queue->tail, 0);
  atomic_init(&queue->wait_empty, 0);
  atomic_init(&queue->wait_full, 0);
  return 0;
}

__attribute__((nonnull))
static inline void mpmc_queue_deinit(struct mpmc_queue *queue)
{
  free(queue->buffer);
}

/*
 * This gives only kind of an approximation of the number of elements in the queue. It may return
 * that a queue is almost empty, but a push operation will fail. This can happen when a thread is
 * scheduled away just before the store to `&cell->sequence` in mpmc_queue_pop(), but
 * other threads will pop many times and increase `head` during that time.
 */
__attribute__((nonnull))
static inline uint32_t mpmc_queue_size(struct mpmc_queue *queue)
{
  uint32_t head, tail;

  head = atomic_load_explicit(&queue->head, memory_order_acquire);
  tail = atomic_load_explicit(&queue->tail, memory_order_relaxed);

  return tail - head;
}

__attribute__((nonnull,warn_unused_result))
static inline bool mpmc_queue_pull(struct mpmc_queue *queue, uint64_t *data_ptr)
{
  struct mpmc_queue_cell *buffer = queue->buffer, *cell;
  uint32_t buffer_mask = queue->buffer_mask, head;
  uint64_t data;

  head = atomic_load_explicit(&queue->head, memory_order_relaxed);
  for (;;) {
    uint32_t seq;
    int32_t diff;

    cell = &buffer[head & buffer_mask];
    seq = atomic_load_explicit(&cell->sequence, memory_order_acquire);
    diff = (int32_t)(seq - (head + 1));
    if (diff == 0) {
      if (atomic_compare_exchange_weak_explicit(&queue->head, &head, head + 1, memory_order_relaxed,
                                                memory_order_relaxed)) {
        break;
      }
    } else if (diff < 0) {
      return false;
    } else {
      head = atomic_load_explicit(&queue->head, memory_order_relaxed);
    }
  }
  data = cell->data;
  atomic_store_explicit(&cell->sequence, head + buffer_mask + 1, memory_order_release);
  *data_ptr = data;
  return true;
}

__attribute__((nonnull,warn_unused_result))
static inline bool mpmc_queue_push(struct mpmc_queue *queue, uint64_t data)
{
  struct mpmc_queue_cell *buffer = queue->buffer, *cell;
  uint32_t buffer_mask = queue->buffer_mask, tail;

  tail = atomic_load_explicit(&queue->tail, memory_order_relaxed);
  for (;;) {
    uint32_t seq;
    int32_t diff;

    cell = &buffer[tail & buffer_mask];
    seq = atomic_load_explicit(&cell->sequence, memory_order_acquire);
    diff = (int32_t)(seq - tail);
    if (diff == 0) {
      if (atomic_compare_exchange_weak_explicit(&queue->tail, &tail, tail + 1, memory_order_relaxed,
                                                memory_order_relaxed)) {
        break;
      }
    } else if (diff < 0) {
      return false;
    } else {
      tail = atomic_load_explicit(&queue->tail, memory_order_relaxed);
    }
  }
  cell->data = data;
  atomic_store_explicit(&cell->sequence, tail + 1, memory_order_release);
  return true;
}

__attribute__((nonnull))
static inline void mpmc_queue_wake_empty(struct mpmc_queue *queue)
{
  unsigned count;
  while ((count = atomic_load_explicit(&queue->wait_empty, memory_order_relaxed))) {
    if (futex_wake(&queue->tail, 1) != 0)
      break;
  }
}

__attribute__((nonnull))
static inline void mpmc_queue_wake_full(struct mpmc_queue *queue)
{
  unsigned count;
  while ((count = atomic_load_explicit(&queue->wait_full, memory_order_relaxed))) {
    if (futex_wake(&queue->head, 1) != 0)
      break;
  }
}

__attribute__((nonnull))
static inline void mpmc_queue_broadcast_empty(struct mpmc_queue *queue)
{
  futex_wake(&queue->tail, INT_MAX);
}

__attribute__((nonnull))
static inline void mpmc_queue_broadcast_full(struct mpmc_queue *queue)
{
  futex_wake(&queue->head, INT_MAX);
}

__attribute__((nonnull,warn_unused_result))
static inline bool mpmc_queue_pull_wait(struct mpmc_queue *queue, uint64_t *data, atomic_bool *running)
{
  for (uint32_t tries = 0; tries < MAX_SPIN; ++tries) {
    if (mpmc_queue_pull(queue, data))
      return true;
  }

  atomic_fetch_add_explicit(&queue->wait_empty, 1, memory_order_relaxed);
  while (atomic_load_explicit(running, memory_order_relaxed)) {
    uint32_t tail = atomic_load_explicit(&queue->tail, memory_order_relaxed);
    if (mpmc_queue_pull(queue, data)) {
      atomic_fetch_sub_explicit(&queue->wait_empty, 1, memory_order_relaxed);
      return true;
    }
    if (futex_wait(&queue->tail, tail, running) < 0)
      break;
  }
  atomic_fetch_sub_explicit(&queue->wait_empty, 1, memory_order_relaxed);

  return false;
}

__attribute__((nonnull,warn_unused_result))
static inline bool mpmc_queue_push_wait(struct mpmc_queue *queue, uint64_t data, atomic_bool *running)
{
  for (uint32_t tries = 0; tries < MAX_SPIN; ++tries) {
    if (mpmc_queue_push(queue, data))
      return true;
  }

  atomic_fetch_add_explicit(&queue->wait_full, 1, memory_order_relaxed);
  while (atomic_load_explicit(running, memory_order_relaxed)) {
    uint32_t head = atomic_load_explicit(&queue->head, memory_order_relaxed);
    if (mpmc_queue_push(queue, data)) {
      atomic_fetch_sub_explicit(&queue->wait_full, 1, memory_order_relaxed);
      return true;
    }
    if (futex_wait(&queue->head, head, running) < 0)
      break;
  }
  atomic_fetch_sub_explicit(&queue->wait_full, 1, memory_order_relaxed);

  return false;
}


#endif
