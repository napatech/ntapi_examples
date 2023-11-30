#ifndef RX_H
#define RX_H

#include <stdint.h>

int rx_init(uint32_t stream[], unsigned stream_count);
void rx_join(void);
void rx_deinit(void);

#endif
