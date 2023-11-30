#include <assert.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <nt.h>

#include "flowtable.h"
#include "stats.h"

#define BUCKET_DYNAMIC 0x01
#define BUCKET_TO_FREE 0x02

#define FLOW_TABLE_SECOND_LEVEL_HASH 0

struct flow_bucket {
  struct flow_chain  *parent;
  struct flow_bucket *prev;
  struct flow_bucket *next;
  pthread_rwlock_t rwlock;
  uint64_t flags;
  uint64_t unused;
  struct flow_entry entries[];
};

struct flow_chain {
  pthread_rwlock_t rwlock;
  struct flow_bucket *last;
  struct flow_bucket first;
};

static void *flow_table = NULL;
static size_t table_size = 0;
static size_t table_mask = 0;
static size_t table_bucket_size = 4;
static size_t table_bucket_mask = (1 << 4) - 1;
static unsigned table_bucket_index_mask = 3;
static size_t table_chain_size = 0;

static void ft_init_bucket(struct flow_bucket *bucket, struct flow_chain *parent, struct flow_bucket *prev, bool dynamic)
{
  pthread_rwlock_init(&bucket->rwlock, NULL);
  if (prev)
    prev->next = bucket;
  bucket->parent = parent;
  bucket->prev = prev;
  bucket->flags = (dynamic ? BUCKET_DYNAMIC : 0);
  bucket->unused = ~(typeof(bucket->unused))0u;
  bucket->next = NULL;

  /* initialise the rwlocks in each entry */
  for (int i = 0; i < table_bucket_size; ++i)
    pthread_rwlock_init(&bucket->entries[i].rwlock, NULL);
}

static struct flow_bucket *ft_add_bucket(struct flow_chain *parent, struct flow_bucket *prev, bool dynamic)
{
  struct flow_bucket *bucket = calloc(1, sizeof(struct flow_bucket) +
      sizeof(struct flow_entry) * table_bucket_size);
  if (!bucket)
    return NULL;
  ft_init_bucket(bucket, parent, prev, dynamic);
  return bucket;
}

int ft_init(size_t power2, size_t chain_size, size_t bucket_size)
{
  int i = 0;

  table_size = 1ull << power2;
  table_mask = table_size - 1;
  table_bucket_size = bucket_size;
  table_bucket_mask = (1ull << bucket_size) - 1;
  table_bucket_index_mask = __builtin_popcount(table_bucket_mask) - 1;

  if (bucket_size < 1 ||
      bucket_size > sizeof(((struct flow_bucket *)0)->unused) * 8 ||
      (bucket_size & (bucket_size - 1))) { /* test if power-of-2 */
    errno = EINVAL;
    return -1;
  }

  table_chain_size = sizeof(struct flow_chain) + sizeof(struct flow_bucket) +
    sizeof(struct flow_entry) * table_bucket_size;
  flow_table = calloc(table_size, table_chain_size);
  if (!flow_table)
    return -1;

  for (; i < table_size; ++i) {
    struct flow_chain *chain = (struct flow_chain *)
      ((uintptr_t)flow_table + i * table_chain_size);
    struct flow_bucket *bucket = &chain->first;
    struct flow_bucket *prev = bucket;
    ft_init_bucket(&chain->first, chain, NULL, false);
    pthread_rwlock_init(&chain->rwlock, NULL);
    for (int j = 1; j < chain_size; ++j) {
      bucket->next = ft_add_bucket(chain, bucket, false);
      if (!bucket->next) {
        goto free_table;
      }
      prev = bucket;
      bucket = bucket->next;
    }
    chain->last = prev;
  }

  return 0;

free_table:
  for (; i >= 0; --i) {
    struct flow_chain *chain = (struct flow_chain *)
      ((uintptr_t)flow_table + i * table_chain_size);
    struct flow_bucket *bucket = chain->first.next;
    struct flow_bucket *next = NULL;
    while (bucket) {
      next = bucket->next;
      free(bucket);
      bucket = next;
    }
  }
  free(flow_table);
  return -1;
}

void ft_deinit(void)
{
  for (int i = table_size - 1; i >= 0; --i) {
    struct flow_chain *chain = (struct flow_chain *)
      ((uintptr_t)flow_table + i * table_chain_size);
    struct flow_bucket *bucket = chain->first.next;
    struct flow_bucket *next = NULL;
    while (bucket) {
      next = bucket->next;
      free(bucket);
      bucket = next;
    }
  }
  free(flow_table);
}

struct flow_entry *ft_add(uint32_t hash, const uint8_t *key_data, uint16_t color)
{
  struct flow_chain *chain = (struct flow_chain *)
      ((uintptr_t)flow_table + (hash & table_mask) * table_chain_size);
  struct flow_bucket *bucket = &chain->first;
  struct flow_entry *entry = NULL;

  /* traverse the bucket chain for the table entry */
  pthread_rwlock_rdlock(&chain->rwlock);
  do {
    pthread_rwlock_rdlock(&bucket->rwlock);
    /* here's a special case: while traversing, the bucket may be in the process of
     * being freed - we need to skip over it. */
    if (!(bucket->flags & BUCKET_TO_FREE) && (bucket->unused & table_bucket_mask)) {
      pthread_rwlock_unlock(&bucket->rwlock);
      /* try to acquire a write lock (since unused is modified) and retest */
      if (!pthread_rwlock_trywrlock(&bucket->rwlock)) {
        if (!(bucket->flags & BUCKET_TO_FREE) && (bucket->unused & table_bucket_mask)) {
          /* find an unused entry (a '1' in the mask) */
          /* we do this by first using the hash, and then using a bitsearch */
          int unused_index;
#if FLOW_TABLE_SECOND_LEVEL_HASH
          unused_index = (hash >> table_mask) & table_bucket_index_mask;
          if ((1 << unused_index) & bucket->unused) {
            bucket->unused &= ~(1 << unused_index);
          } else {
#endif
            unused_index = __builtin_ctz(bucket->unused);
            assert(unused_index < table_bucket_size);
            assert((1 << unused_index) & bucket->unused);
            bucket->unused &= (bucket->unused - 1); /* reset lowest bit */
#if FLOW_TABLE_SECOND_LEVEL_HASH
          }
#endif
          entry = bucket->entries + unused_index;
          entry->index = unused_index;
          /* release the bucket mutex so that other threads can
           * traverse through the bucket to add new flow
           * entries or remove them */
          pthread_rwlock_unlock(&bucket->rwlock);
          /* now continue to initialise the entry */
          goto init_entry;
        } else {
          /* edge-case: lock acquired but marked as free */
          pthread_rwlock_unlock(&bucket->rwlock);
        }
      }
      continue;
    }
    pthread_rwlock_unlock(&bucket->rwlock);
  } while ((bucket = bucket->next));

  /* if there are no more buckets, we allocate one */
  if (!bucket) {
    /* no longer traversing, we exchange for a write lock, and
     * just append a new bucket to the end of the chain */
    pthread_rwlock_unlock(&chain->rwlock);
    pthread_rwlock_wrlock(&chain->rwlock);
    bucket = ft_add_bucket(chain, chain->last, true);
    chain->last->next = bucket;
    chain->last = bucket;
    if (!bucket) {
      pthread_rwlock_unlock(&chain->rwlock);
      return NULL;
    }
  }

  /* for new buckets, we allocate the first entry;
   * note that since this is a new bucket, it's not
   * visible until the chain is unlocked (wrlock held) */
  int unused_index;
#if FLOW_TABLE_SECOND_LEVEL_HASH
  unused_index = (hash >> table_mask) & table_bucket_index_mask;
  bucket->unused = ~(1 << unused_index);
#else
  unused_index = 0;
  bucket->unused = ~1;
#endif

  entry = bucket->entries + unused_index;
  entry->index = unused_index;
  pthread_rwlock_init(&entry->rwlock, NULL);

#ifdef LEARNDBG
  fprintf(stderr, "warning: new bucket allocated for entry %p\n", entry);
#endif

init_entry:
  /* we need to lock the entry because we haven't finished
   * initialising it yet; but we don't need the chain nor
   * the bucket anymore because we mark the entry as 'used'
   * in the bucket so any readers for the entry
   * details will have to wait, but bucket traversals
   * (e.g. for allocating new entries), will work */
  pthread_rwlock_wrlock(&entry->rwlock);

  /* unlock the chain because we don't need it anymore */
  pthread_rwlock_unlock(&chain->rwlock);

  entry->bucket = bucket;
  entry->hash = hash;
  entry->flags = 0;
  entry->color = color;
  fi_init(&entry->info);

  memcpy(entry->key_data, key_data,
         (color & FLOW_COLOR_IPV6) ? FLOW_IPV6_KEY_LEN : FLOW_IPV4_KEY_LEN);
  pthread_rwlock_unlock(&entry->rwlock);

  stats_update(STAT_LEARN, 1);
  stats_update(STAT_FLOW, 1);

  /* start with one reference */
  ft_ref(entry, 1);

#ifdef LEARNDBG
  char src[16], dst[16];
  uint16_t srcport, dstport;
  srcport = (entry->key_data[8] << 8) | entry->key_data[9];
  dstport = (entry->key_data[10] << 8) | entry->key_data[11];
  snprintf(src, sizeof(src), "%d.%d.%d.%d", entry->key_data[0], entry->key_data[1], entry->key_data[2], entry->key_data[3]);
  snprintf(dst, sizeof(dst), "%d.%d.%d.%d", entry->key_data[4], entry->key_data[5], entry->key_data[6], entry->key_data[7]);
  fprintf(stderr, "ftadd: %p hash:%08x src:%s:%u dst:%s:%u color:%04x stream_id=%u\n", entry, entry->hash, src, srcport, dst, dstport, entry->color, entry->stream_id);
#endif

  return entry;
}

void ft_remove(struct flow_entry *entry)
{
  struct flow_bucket *bucket = entry->bucket;
  bool to_free = false;

#ifdef MEMDEBUG
  fprintf(stderr, "remove: %p\n", entry);
#endif

  /* sometimes flow records come for elements that
   * are already removed; we can silently ignore if
   * this is the case */
  pthread_rwlock_rdlock(&bucket->rwlock);
  if (bucket->unused & (1 << entry->index)) {
    pthread_rwlock_unlock(&bucket->rwlock);
    return;
  }
  pthread_rwlock_unlock(&bucket->rwlock);

  /* temporarily hold the bucket and only mark it
   * for deletion first (to avoid having nesting
   * and pri-inversion that could cause deadlocks) */
  pthread_rwlock_wrlock(&bucket->rwlock);
  bucket->unused |= (1 << entry->index);
  if ((to_free = (bucket->flags & BUCKET_DYNAMIC) &&
      !((~bucket->unused) & table_bucket_mask))) {
    assert(bucket->flags & BUCKET_DYNAMIC);
    assert(bucket->prev);
    bucket->flags |= BUCKET_TO_FREE;
  }
  pthread_rwlock_unlock(&bucket->rwlock);

  if (to_free) {
    /* lock the chain to remove this bucket */
    struct flow_chain *chain = bucket->parent;
    pthread_rwlock_wrlock(&chain->rwlock);
    if (chain->last == bucket)
      chain->last = bucket->prev;
    if (bucket->prev)
      bucket->prev->next = bucket->next;
    if (bucket->next)
      bucket->next->prev = bucket->prev;
    pthread_rwlock_unlock(&chain->rwlock);

    free(bucket);
  }

  stats_update(STAT_UNLEARN, 1);
  stats_update(STAT_FLOW, -1);
}

static int ft_compare(const struct flow_entry *entry, uint32_t hash, const uint8_t *key_data, uint32_t color)
{
  int match = (entry->hash != hash);

  match += ((entry->color & FLOW_COLOR_TYPE_MASK) != (color & FLOW_COLOR_TYPE_MASK));
  match += !!memcmp(entry->key_data, key_data,
                  (color & FLOW_COLOR_IPV6) ? FLOW_IPV6_KEY_LEN : FLOW_IPV4_KEY_LEN);

  return match;
}

static struct flow_entry *ft_visit(uint32_t hash, int (* visitor)(uint32_t hash, const struct flow_entry *, va_list), ...)
{
  struct flow_chain *chain = (struct flow_chain *)
      ((uintptr_t)flow_table + (hash & table_mask) * table_chain_size);
  struct flow_bucket *bucket;
  struct flow_entry *entry = NULL;
  bool not_found = true;
  va_list args;

  va_start(args, visitor);

  /* traverse the bucket chain for the table entry */
  pthread_rwlock_rdlock(&chain->rwlock);
  bucket = &chain->first;
  do {
    int used_index;
    typeof(bucket->unused) used;
    pthread_rwlock_rdlock(&bucket->rwlock);
    used = (~bucket->unused) & table_bucket_mask;

#if FLOW_TABLE_SECOND_LEVEL_HASH
    /* lookup the bitmask using the hash first */
    used_index = (hash >> table_mask) & table_bucket_index_mask;
    if ((1 << used_index) & used) {
      used &= ~(1 << used_index);
      entry = bucket->entries + used_index;
      not_found = !!visitor(hash, entry, args);
    }
#endif

    /* scan if the entry did not match */
    while (not_found && used) {
      /* index of lowest indexed bucket entry */
      used_index = __builtin_ctz(used);
      assert(used_index < table_bucket_size);
      entry = bucket->entries + used_index;
      assert(used_index == entry->index);
      //fprintf(stderr, "visit: %p hash:%08x\n", e, e->hash);
      not_found = !!visitor(hash, entry, args);
      /* reset lowest set bit */
      used &= (used - 1);
    }

    pthread_rwlock_unlock(&bucket->rwlock);
  } while (not_found && (bucket = bucket->next));
  pthread_rwlock_unlock(&chain->rwlock);

  va_end(args);
  return not_found ? NULL : entry;
}

static int ft_lookup_visitor(uint32_t hash, const struct flow_entry *entry, va_list a)
{
  const uint8_t *key_data;
  int ret;
  va_list args;
  uint32_t color;

  va_copy(args, a);
  key_data = va_arg(args, const uint8_t *);
  color = va_arg(args, uint32_t);

  ret = ft_compare(entry, hash, key_data, color);
  va_end(args);
  return ret;
}

__attribute__((flatten))
struct flow_entry *ft_lookup(uint32_t hash, const uint8_t *key_data, uint16_t color)
{
  return ft_visit(hash, ft_lookup_visitor, key_data, color);
}

static int ft_validate_visitor(uint32_t hash, const struct flow_entry *entry, va_list a)
{
  int ret;
  va_list args;
  const struct flow_entry *other;

  va_copy(args, a);
  other = va_arg(args, const struct flow_entry *);
  ret = (entry != other);
  va_end(args);

  return ret;
}

__attribute__((flatten))
int ft_validate(uint32_t hash, const struct flow_entry *entry)
{
  return ft_visit(hash, ft_validate_visitor, entry) == NULL;
}

void ft_ref(struct flow_entry *entry, uint32_t count)
{
  atomic_fetch_add(&entry->rc, count);
}

void ft_deref(struct flow_entry *entry, uint32_t count)
{
  int32_t pre_count;
  if ((pre_count = atomic_fetch_sub(&entry->rc, count)) <= count) {
    assert(pre_count - count >= 0);
#ifdef MEMDEBUG
    fprintf(stderr, "remove: %p\n", entry);
#endif
    ft_remove(entry);
  }
}
