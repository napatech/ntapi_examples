#ifndef NTPL_H
#define NTPL_H

#include <stdint.h>

int ntpl_init(int verbose);
int ntpl_common_apply(void);
int ntpl_apply(uint32_t streams[], unsigned stream_count, unsigned upstream, unsigned downstream);
void ntpl_deinit(void);

#endif
