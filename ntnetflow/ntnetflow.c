#include <ctype.h>
#include <string.h>
#include <stdatomic.h>
#include <stdarg.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <sys/param.h>

#include <pthread.h>
#include <nt.h>

#include "common.h"
#include "argparse.h"
#include "info.h"
#include "signal.h"
#include "ntpl.h"
#include "flowlearn.h"
#include "flowtable.h"
#include "flowstats.h"
#include "rx.h"
#include "stats.h"

static int opt_verbose = 0;
static int opt_quiet = 0;
static int opt_stat = 0;
static int opt_csv = 0;
static int opt_emulate_probe = 0;
static int opt_upstream = 0;
static int opt_downstream = 1;
static int opt_stat_interval = 1000;
static int opt_poll_interval = 10;
static int opt_power = 10;
static int opt_stream_count = 4;
static int opt_chain_size = 1;
static int opt_bucket_size = 4;

struct argparse_option arg_options[] = {
  OPT_HELP(),
  OPT_BOOLEAN('v', "verbose",         &opt_verbose,         "Verbose output", NULL, 0, 0, NULL),
  //OPT_BOOLEAN('q', "quiet",           &opt_quiet,           "Supressed output (for benchmarking purpose)", NULL, 0, 0, NULL),
  OPT_BOOLEAN('s', "stat",            &opt_stat,            "Print live statistics", NULL, 0, 0, NULL),
  OPT_BOOLEAN('c', "csv",             &opt_csv,             "Print raw statistics in CSV", NULL, 0, 0, NULL),
  OPT_BOOLEAN('e', "emulate-probing", &opt_emulate_probe,   "Force probing emulation on adapters that have HW probing", NULL, 0, 0, NULL),
  OPT_INTEGER('u', "upstream-port",   &opt_upstream,        "Upstream port number used for ntnetflow\ndefault is port 0", NULL, 0, 0, "upstream port number"),
  OPT_INTEGER('d', "downstream-port", &opt_downstream,      "Downstream port number used for ntnetflow\ndefault is port 1", NULL, 0, 0, "downstream port number"),
  OPT_INTEGER('n', "stream-count",    &opt_stream_count,    "Number of threads receiving from NT hostbuffers", NULL, 0, 0, "thread count"),
  OPT_INTEGER('i', "interval",        &opt_stat_interval,   "Statistics interval, default is 1000 ms", NULL, 0, 0, "milliseconds"),
  OPT_INTEGER('p', "poll",            &opt_poll_interval,   "Polling interval for netflow records, default is 10s", NULL, 0, 0, "seconds"),
  OPT_INTEGER('x', "flowtable-power", &opt_power,           "Size of the software flow table in power-of-two, default is 10", NULL, 0, 0, "(1 << power)"),
  OPT_INTEGER('a', "chain-size",      &opt_chain_size,      "Number of initial buckets per chain, default is 1", NULL, 0, 0, "chain size"),
  OPT_INTEGER('b', "bucket-size",     &opt_bucket_size,     "Number of entries per bucket, default is 4", NULL, 0, 0, "bucket size"),
  OPT_END(),
};

const char *usage_text[] = {"", NULL};

int main(int argc, const char **argv)
{
  int status = EXIT_FAILURE;
  char buffer[NT_ERRBUF_SIZE];
  struct argparse argparse;
  uint32_t streams[256];
  ssize_t stream_count;
  unsigned adapter_no;
  int efd = -1;
  int fl_flags = 0;
  int mbits = vmembits();

  fprintf(stderr, "%s (v. %s)\n", PROGNAME, TOOLS_RELEASE_VERSION);

  /* sanity checks */
  if (mbits != VIRTUAL_MEM_BITS) {
    if (mbits >= 0) {
      fprintf(stderr, "error: detected %d-bit virtual address size, but compiled for %d\n", mbits, VIRTUAL_MEM_BITS);
      return 1;
    } else
      fprintf(stderr, "error: compiled for %d, but detection not supported on this platform\n", VIRTUAL_MEM_BITS);
  }

  argparse_init(&argparse, arg_options, usage_text, 0);
  argparse_parse(&argparse, argc, argv);

#if MEMDEBUG
  fprintf(stderr, "warning: MEMDEBUG build; performance suboptimal\n");
#endif

  if (opt_stat && opt_csv) {
    fprintf(stderr, "error: cannot use --stat and --csv at the same time\n");
    goto out;
  }

  /* initialize the NTAPI library and thereby check if NTAPI_VERSION
   * can be used together with this library */
  if ((status = NT_Init(NTAPI_VERSION)) != NT_SUCCESS) {
    NT_ExplainError(status, buffer, sizeof(buffer));
    fprintf(stderr, "NT_Init() failed: %s\n", buffer);
    goto out;
  }

  if (info_init() < 0)
    goto done;

  if ((status = info_system_num_ports()) < 0)
    goto deinit_info;

  if (opt_upstream < 0 || opt_upstream > status - 1) {
    fprintf(stderr, "error: illegal upstream port number specified,"
                    " it must be in the range [0, %d].\n\n", status-1);
    goto deinit_info;
  }

  if (opt_downstream < 0 || opt_downstream > status - 1) {
    fprintf(stderr, "error: illegal downstream port number specified,"
                    " it must be in the range [0, %d].\n\n", status-1);
    goto deinit_info;
  }

  if ((adapter_no = info_port_adapter_no(opt_upstream)) !=
       info_port_adapter_no(opt_downstream)) {
    fprintf(stderr, "error: upstream and downstream port must reside"
                    " on the same physical adapter.\n\n");
    goto deinit_info;
  }

  /* initialise the HW flow stats */
  if (fs_init(adapter_no) < 0)
    goto deinit_info;

  /* currently we need at least the flow matcher available;
   * TODO: implement complete software fallback */
  if (fs_version() < 0) {
    fprintf(stderr, "error: this adapter does not support FLM\n");
    goto deinit_fs;
  }

  if ((status = info_adapter_flowstatus_enabled(adapter_no,
                                                fs_probe_supported())) < 0)
    goto deinit_fs;

  if (!status) {
    fprintf(stderr, "error: FlowStatus records are disabled in ntservice.ini\n"
                    "Add the following to to your adapter section:\n"
                    "\tFlowStatusLearnDone = True\n"
                    "\tFlowStatusLearnFail = True\n"
                    "\tFlowStatusLearnIgnore = True\n"
                    "\tFlowStatusUnlearnDone = True\n"
                    "\tFlowStatusUnlearnIgnore = True\n%s\n",
                    fs_probe_supported() ?
                    "\tFlowStatusProbeDone = True\n"
                    "\tFlowStatusProbeIgnore = True\n" : "");
    goto deinit_fs;
  }

  if ((status = info_adapter_timestamp_type(adapter_no)) < 0)
    goto deinit_fs;
  else {
    enum NtTimestampType_e ts_type = status;
    const char *ts_name = "UNKNOWN";

    switch (ts_type) {
      case NT_TIMESTAMP_TYPE_NATIVE_UNIX:   ts_name = "NATIVE_UNIX";   break;
      case NT_TIMESTAMP_TYPE_UNIX_NANOTIME: ts_name = "UNIX_NANOTIME"; break;
      case NT_TIMESTAMP_TYPE_PCAP:          ts_name = "PCAP";
      case NT_TIMESTAMP_TYPE_PCAP_NANOTIME: ts_name = "PCAP_NANOTIME";
      case NT_TIMESTAMP_TYPE_NATIVE:        ts_name = "NATIVE";
      case NT_TIMESTAMP_TYPE_NATIVE_NDIS:   ts_name = "NATIVE_NDIS";
      default:
        fprintf(stderr, "error: unsupported timestamp type: %s\n\n", ts_name);
        goto deinit_fs;
    }
  }

  if (opt_stream_count < 1 || opt_stream_count > 128) {
    fprintf(stderr, "error: stream count must be in the range between [1,128].\n\n");
    goto deinit_fs;
  }

  if (opt_power < FT_POWER_LB || opt_power > FT_POWER_UB) {
    fprintf(stderr, "error: flow table power must be in range between ["
                    XSTR(FT_POWER_LB) "," XSTR(FT_POWER_UB) "].\n\n");
    goto deinit_fs;
  }

  if (opt_chain_size < 1 || opt_chain_size > 32) {
    fprintf(stderr, "error: chain size must be in range between [1,32].\n\n");
    goto deinit_fs;
  }

  /* test if bucket size is positive, power-of-two, under 32 */
  if (opt_bucket_size < 1 || opt_bucket_size > 32 || (opt_bucket_size & (opt_bucket_size - 1))) {
    fprintf(stderr, "error: bucket size must be one of 1,2,4,8,16,32.\n\n");
    goto deinit_fs;
  }

  /* allocate some streams */
  if ((stream_count = info_streams_unused(streams, (size_t)opt_stream_count)) < 0)
    goto deinit_fs;

  if (stream_count < opt_stream_count) {
    fprintf(stderr, "error: could only allocate %zi streams when %d requested\n", stream_count, opt_stream_count);
    goto deinit_fs;
  }

  /* initialise signal handlers */
  if ((efd = signal_init()) < 0)
    goto deinit_fs;

  /* initialise stats signals and timers */
  if ((opt_stat || opt_csv) && ((stats_init(opt_stat_interval) < 0) || (stats_start() < 0))) {
    fprintf(stderr, "error: failed to initialise statistics\n");
    goto deinit_signal;
  }

  /* set up the ntpl */
  if (ntpl_init(opt_verbose) < 0)
    goto deinit_stats;

  /* apply the ntpl; common and dynamic ntpl */
  if (ntpl_common_apply() < 0)
    goto deinit_ntpl;

  if (ntpl_apply(streams, stream_count, opt_upstream, opt_downstream) < 0)
    goto deinit_ntpl;

  /* initialise the flow table */
  if (ft_init(opt_power, opt_chain_size, opt_bucket_size) < 0)
    goto deinit_ntpl;

  /* initialise the flow learner, scrubber and recorder */
  fl_flags = opt_verbose ? FL_FLAGS_VERBOSE : 0;
  if (!opt_emulate_probe)
    fl_flags |= fs_probe_supported() ? FL_FLAGS_HW_PROBE : 0;
  if (fl_init(adapter_no, opt_poll_interval, fl_flags) < 0)
    goto deinit_ft;

  /* initialise the stream readers */
  if (rx_init(streams, stream_count)) {
    fl_join();
    goto deinit_fl;
  }

  if (!opt_quiet) {
    fprintf(stderr, "flm version: %d\n", fs_version());
    fprintf(stderr, "flow entry size: %zu\n", sizeof(struct flow_entry));
  }

  while (true) {
    usleep(opt_stat_interval * 1000);
    if (!atomic_load_explicit(&app_running, memory_order_relaxed))
      break;
    if (opt_stat)
      stats_fprintf(stdout, '\r');
    else if (opt_csv)
      stats_csv_fprintf(stdout, '\n');
  }

  /* so that final messages don't land in the stats line */
  if (opt_stat)
    puts("");

  status = EXIT_SUCCESS;

  fl_join();
  rx_join();

  rx_deinit();
deinit_fl:
  fl_deinit();
deinit_ft:
#if 0
  /* this is technically not necessary because on *nix, memory is freed on application
   * termination.  Currently flowtable memory allocation/deallocation is not optimised */
  ft_deinit();
#endif
deinit_ntpl:
  ntpl_deinit();
deinit_stats:
  stats_deinit();
deinit_signal:
  signal_deinit();
deinit_fs:
  fs_deinit();
deinit_info:
  info_deinit();
done:
  NT_Done();
out:
  return status;
}
