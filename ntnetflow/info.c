#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "info.h"

static _Thread_local NtInfoStream_t info_stream = NULL;
static _Thread_local NtInfo_t cached = {0};

void info_clear_cache(void)
{
  cached.cmd = NT_INFO_CMD_READ_UNKNOWN;
}

int info_init(void)
{
  char buffer[NT_ERRBUF_SIZE];
  int status = NT_InfoOpen(&info_stream, "osmode-info-stream");
  if (status) {
    NT_ExplainError(status, buffer, sizeof(buffer));
    fprintf(stderr, "error: NT_InfoOpen failed: %s\n", buffer);
    return -1;
  }
  return status;
}

int info_deinit(void)
{
  char buffer[NT_ERRBUF_SIZE];
  int status = NT_InfoClose(info_stream);
  if (status) {
    NT_ExplainError(status, buffer, sizeof(buffer));
    fprintf(stderr, "error: NT_InfoClose failed: %s\n", buffer);
    return -1;
  }
  return status;
}

static int info_read(NtInfo_t *info, const char *error_src)
{
  char buffer[NT_ERRBUF_SIZE];
  int status = NT_InfoRead(info_stream, info);
  if (status != NT_SUCCESS) {
    NT_ExplainError(status, buffer, sizeof(buffer));
    fprintf(stderr, "error: unable to read info%s: %s\n", error_src ? error_src : "", buffer);
    return -1;
  }
  return 0;
}

static int info_read_adapter(unsigned adapter, struct NtInfo_s *info)
{
  char error_src[32];
  snprintf(error_src, sizeof(error_src), " for adapter %d", adapter);
  if (info == &cached &&
      info->cmd == NT_INFO_CMD_READ_ADAPTER &&
      info->u.adapter.adapterNo == (uint8_t)adapter) {
    return 0;
  }
  memset(info, 0, sizeof(*info));
  info->cmd = NT_INFO_CMD_READ_ADAPTER_V6;
  info->u.adapter_v6.adapterNo = (uint8_t)adapter;
  return info_read(info, error_src);
}

static int info_read_port(unsigned port, struct NtInfo_s *info)
{
  char error_src[32];
  snprintf(error_src, sizeof(error_src), " for port %d", port);
  if (info == &cached &&
      info->cmd == NT_INFO_CMD_READ_PORT &&
      info->u.port.portNo == (uint8_t)port) {
    return 0;
  }
  memset(info, 0, sizeof(*info));
  info->cmd = NT_INFO_CMD_READ_PORT;
  info->u.port.portNo = (uint8_t)port;
  return info_read(info, error_src);
}

int info_read_system(struct NtInfo_s *info)
{
  char error_src[32];
  snprintf(error_src, sizeof(error_src), " for streams");
  memset(info, 0, sizeof(*info));
  info->cmd = NT_INFO_CMD_READ_SYSTEM;
  return info_read(info, error_src);
}

static int info_read_streams(struct NtInfo_s *info)
{
  char error_src[32];
  snprintf(error_src, sizeof(error_src), " for streams");
  memset(info, 0, sizeof(*info));
  info->cmd = NT_INFO_CMD_READ_STREAM;
  return info_read(info, error_src);
}

int info_port_mac_address(unsigned port, struct ether_addr *addr)
{
  if (info_read_port(port, &cached) < 0)
    return -1;

  memcpy(addr->ether_addr_octet, cached.u.port.data.macAddress, ETH_ALEN);
  return 0;
}

int info_port_max_tx_pkt_size(unsigned port)
{
  if (info_read_port(port, &cached) < 0)
    return -1;

  return cached.u.port.data.capabilities.maxTxPktSize;
}

int info_port_min_tx_pkt_size(unsigned port)
{
  if (info_read_port(port, &cached) < 0)
    return -1;

  return cached.u.port.data.capabilities.minTxPktSize;
}

int info_port_link_up(unsigned port)
{
  static NtInfo_t info;

  if (info_read_port(port, &info) < 0)
    return -1;

  return (info.u.port.data.state == NT_LINK_STATE_UP);
}

int info_port_adapter_no(unsigned port)
{
  if (info_read_port(port, &cached) < 0)
    return -1;

  return cached.u.port.data.adapterNo;
}

int info_adapter_profile_capture(unsigned adapter)
{
  if (info_read_adapter(adapter, &cached) < 0)
    return -1;

  return (cached.u.adapter_v6.data.profile == NT_PROFILE_TYPE_CAPTURE);
}

int info_adapter_timestamp_type(unsigned adapter)
{
  if (info_read_adapter(adapter, &cached) < 0)
    return -1;

  return (cached.u.adapter_v6.data.timestampType);
}

int info_system_num_ports(void)
{
  if (info_read_system(&cached) < 0)
    return -1;

  return cached.u.system.data.numPorts;
}

ssize_t info_streams_active(uint32_t *out, size_t max)
{
  static NtInfo_t info;

  assert(sizeof(out[0]) == sizeof(info.u.stream.data.streamIDList[0]));

  if (info_read_streams(&info) < 0)
    return -1;

  if (max > info.u.stream.data.count)
    max = info.u.stream.data.count;

  memcpy(out, info.u.stream.data.streamIDList, max * sizeof(out[0]));

  return (ssize_t)max;
}

static int cmpid(const void *pa, const void *pb)
{
  const int *a = pa, *b = pb;
  return *a - *b;
}

ssize_t info_streams_unused(uint32_t *out, size_t max)
{
  NtInfo_t info;
  unsigned j = 0, k = 0;
  int last = 0;

  assert(sizeof(out[0]) == sizeof(info.u.stream.data.streamIDList[0]));

  if (info_read_streams(&info) < 0)
    return -1;

  if (max > 256 - info.u.stream.data.count)
    max = 256 - info.u.stream.data.count;

  qsort(info.u.stream.data.streamIDList, info.u.stream.data.count,
        sizeof(info.u.stream.data.streamIDList[0]), cmpid);

  while (j < max && k < info.u.stream.data.count) {
    for (int i = last; i < info.u.stream.data.streamIDList[k] && j < max; ++i)
      out[j++] = (unsigned)i;
    last = info.u.stream.data.streamIDList[k] + 1;
    ++k;
  }
  for (int i = last; i < 256 && j < max; ++i)
    out[j++] = (unsigned)i;

  return (ssize_t)max;
}

static int info_read_property(struct NtInfo_s *info, const char *error_src, const char *format, ...)
{
  memset(info, 0, sizeof(*info));
  info->cmd = NT_INFO_CMD_READ_PROPERTY;
  va_list args;
  va_start(args, format);
  vsnprintf(info->u.property.path, sizeof(info->u.property.path), format, args);
  va_end(args);
  return info_read(info, error_src);
}

static int info_read_ini_adapter_property(unsigned adapter, const char *property, struct NtInfo_s *info)
{
  char error_src[32];
  snprintf(error_src, sizeof(error_src), " for adapter %u ini", adapter);
  return info_read_property(info, error_src, "ini.Adapter%u.%s", adapter, property);
}

int info_adapter_tx_segment_size(unsigned adapter)
{
  NtInfo_t info;

  if (info_read_ini_adapter_property(adapter, "HostBufferSegmentSizeTx", &info) < 0)
    return -1;

  return (info.u.property.data.u.i == -1) ? (1024 * 1024) : info.u.property.data.u.i;
}

int info_adapter_rx_segment_size(unsigned adapter)
{
  NtInfo_t info;

  if (info_read_ini_adapter_property(adapter, "HostBufferSegmentSizeRx", &info) < 0)
    return -1;

  return (info.u.property.data.u.i == -1) ? 0 : info.u.property.data.u.i;
}

int info_adapter_flowstatus_enabled(unsigned adapter, bool probe_supported)
{
  const char *settings[] = {
    "FlowStatusLearnDone",
    "FlowStatusLearnFail",
    "FlowStatusLearnIgnore",
    "FlowStatusUnlearnDone",
    "FlowStatusUnlearnIgnore",
    "FlowStatusProbeDone",
    "FlowStatusProbeIgnore",
    NULL
  };

  int enabled = 1;

  for (const char **s = settings; *s; ++s) {
    NtInfo_t info;
    if (!probe_supported && strstr(*s, "Probe"))
      continue;
    if (info_read_ini_adapter_property(adapter, *s, &info) < 0)
      return -1;
    enabled = enabled && info.u.property.data.u.i;
  }
  return enabled;
}
