#ifndef FLOWSTATS_H
#define FLOWSTATS_H

#include <stdbool.h>

int fs_init(unsigned adapter_no);
bool fs_probe_supported(void);
int fs_version(void);
void fs_deinit(void);

#endif
