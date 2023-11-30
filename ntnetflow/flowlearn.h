#ifndef FLOW_LEARN_H
#define FLOW_LEARN_H

#include <stdatomic.h>

#define FL_FLAGS_VERBOSE  (1 << 0)
#define FL_FLAGS_HW_PROBE (1 << 1)

#define FL_OP_UNLEARN 0
#define FL_OP_LEARN   1
#define FL_OP_PROBE   2
#define FL_OP_REFRESH 3

struct flow_entry;

int fl_init(unsigned adapter_no, unsigned interval, uint32_t flags);
int fl_program(const struct flow_entry *entry, unsigned op, ...);
void fl_join(void);
void fl_deinit(void);

struct flow_poll_list {
  struct flow_entry *prev, *next;
  atomic_long skip;
  uint64_t next_update;
};

#endif
