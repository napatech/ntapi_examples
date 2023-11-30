#include "flowstats.h"

#include <nt.h>

static int flowstat_version = -1;
static NtStatStream_t stat_stream = NULL;

int fs_init(unsigned adapter_no)
{
  char buffer[NT_ERRBUF_SIZE];
  int status;

  NtStatistics_t stat[3] = {
    {
      .cmd = NT_STATISTICS_READ_CMD_FLOW_V0,
      .u.flowData_v0.clear = 1,
      .u.flowData_v0.adapterNo = adapter_no,
    },
    {
      .cmd = NT_STATISTICS_READ_CMD_FLOW_V1,
      .u.flowData_v1.clear = 1,
      .u.flowData_v1.adapterNo = adapter_no,
    },
    {
      .cmd = NT_STATISTICS_READ_CMD_FLOW_V2,
      .u.flowData_v2.clear = 1,
      .u.flowData_v2.adapterNo = adapter_no,
    }
  };
  int count = sizeof(stat)/sizeof(stat[0]);

  status = NT_StatOpen(&stat_stream, "ntnetflow");
  if (status != NT_SUCCESS) {
    NT_ExplainError(status, buffer, sizeof(buffer));
    fprintf(stderr, "error: NT_StatOpen failed: %s\n", buffer);
    return -1;
  }

  /* detect flowstat version and read the statistics counters
   * in order to clear them */
  for (flowstat_version = count - 1; flowstat_version >= 0; --flowstat_version) {
    status = NT_StatRead(stat_stream, stat + flowstat_version);
    if (status == NT_SUCCESS) {
      return 0;
    } else if (status != NT_ERROR_FEATURE_NOT_SUPPORTED &&
               status != NT_ERROR_NTAPI_STAT_GET_INVALID_CMD) {
      NT_ExplainError(status, buffer, sizeof(buffer));
      fprintf(stderr, "error: NT_StatRead failed: %s\n", buffer);
      return -1;
    }
  }

  return 0;
}

bool fs_probe_supported(void)
{
  return flowstat_version >= 2;
}

int fs_version(void)
{
  return flowstat_version;
}

void fs_deinit(void)
{
  if (stat_stream)
    NT_StatClose(stat_stream);
}
