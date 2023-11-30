#ifndef INFO_H
#define INFO_H

#include "nt.h"
#include <stdint.h>
#include <stdbool.h>
#include <net/ethernet.h>

int info_init(void);
int info_deinit(void);
void info_clear_cache(void);

int info_port_mac_address(unsigned port, struct ether_addr *addr);
int info_port_link_up(unsigned port);
int info_port_adapter_no(unsigned port);
int info_port_min_tx_pkt_size(unsigned port);
int info_port_max_tx_pkt_size(unsigned port);
int info_system_num_ports(void);
ssize_t info_streams_active(uint32_t *out, size_t count);
ssize_t info_streams_unused(uint32_t *out, size_t count);
int info_read_system(struct NtInfo_s *info);
int info_adapter_timestamp_type(unsigned adapter);
int info_adapter_tx_segment_size(unsigned adapter);
int info_adapter_rx_segment_size(unsigned adapter);
int info_adapter_flowstatus_enabled(unsigned adapter, bool probe_supported);

#endif
