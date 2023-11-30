#ifndef FLOWKEY_H
#define FLOWKEY_H

#include <stdlib.h>
#include <stdbool.h>

#define FLOW_COLOR_TYPE_MASK    0xff
#define FLOW_COLOR_DOWNSTREAM   (1 << 0)
#define FLOW_COLOR_IPV6         (1 << 1)
#define FLOW_COLOR_TCP          (1 << 2)
#define FLOW_COLOR_MONITOR      (1 << 3)
#define FLOW_COLOR_MISS         (1 << 9)
#define FLOW_COLOR_UNHANDLED    (1 << 10)
#define FLOW_COLOR_OTHER        (1 << 11)

#define FLOW_MAX_KEY_LEN        40
#define FLOW_IPV4_KEY_LEN       12
#define FLOW_IPV6_KEY_LEN       36

#define FLOW_KEY_ID_IPV4         1
#define FLOW_KEY_ID_IPV6         2

#define FLOW_KEYSET_ID_BYPASS    3
#define FLOW_KEYSET_ID_MONITOR   4

static inline void fk_create(uint8_t *dst, const uint8_t *l3, const uint8_t *l4, uint8_t src_color)
{
  bool downstream = (src_color & FLOW_COLOR_DOWNSTREAM);

  //memset(dst, 0, FLOW_MAX_KEY_LEN);

  /* ensure the keys is the same on upstream and downstream by swapping
   * the IPs and ports */
  if (src_color & FLOW_COLOR_IPV6) {
    memcpy(dst + (downstream ? 16 :  0), l3 +  8, 16);
    memcpy(dst + (downstream ?  0 : 16), l3 + 24, 16);
    memcpy(dst + (downstream ? 34 : 32), l4 +  0,  2);
    memcpy(dst + (downstream ? 32 : 34), l4 +  2,  2);
  } else {
    memcpy(dst + (downstream ?  4 :  0), l3 + 12,  4);
    memcpy(dst + (downstream ?  0 :  4), l3 + 16,  4);
    memcpy(dst + (downstream ? 10 :  8), l4 +  0,  2);
    memcpy(dst + (downstream ?  8 : 10), l4 +  2,  2);
  }
}

//static inline int fk_keyset_id(uint8_t flags)
//{
//  if (color & FLOW_COLOR_MONITOR)
//    return FLOW_KEYSET_ID_MONITOR;
//  if (color & FLOW_COLOR_BYPASS)
//    return FLOW_KEYSET_ID_BYPASS;
//  return -1;
//}
//
static inline int fk_key_id(uint8_t color)
{
  return (color & FLOW_COLOR_IPV6) ? FLOW_KEY_ID_IPV6 : FLOW_KEY_ID_IPV4;
}

static inline int fk_proto(uint8_t color)
{
  return (color & FLOW_COLOR_TCP) ? 0x06 : 0x11;
}

#endif
