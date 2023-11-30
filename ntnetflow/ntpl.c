#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <unistd.h>
#include <nt.h>

#include "common.h"
#include "info.h"
#include "ntpl.h"
#include "flowkey.h"
#include "ntpl_common_config.h"
#include "ntpl_config.h"

#define MAX_NTPL 1024

static NtConfigStream_t config_stream;
static unsigned ntpl_ids[MAX_NTPL] = {0};
static int ntpl_ids_count = 0;
static pid_t pid = 0;
static char pid_str[8] = {0};
static int ntpl_verbose = 0;

/* performs in-place replacement of XXXXX with hex pid_str.
 * terminates either on null-char or sz reached (sanity) */
static void ntpl_replace_xxxxx(char *buffer, size_t sz, char *xxxxx)
{
  char *p = buffer;
  size_t nb = strlen(xxxxx);
  while (*p && ((size_t)(p - buffer) < sz - nb)) {
    if (*p == 'X') {
      if (!memcmp(p, "XXXXX", nb)) {
        memcpy(p, xxxxx, nb);
        p += 7;
      } else
        ++p;
    } else
      ++p;
  }
}

int ntpl_init(int verbose)
{
  int status;
  char buffer[NT_ERRBUF_SIZE];

  ntpl_verbose = verbose;

  if ((status = NT_ConfigOpen(&config_stream, "osmode-config-stream")) != NT_SUCCESS) {
    NT_ExplainError(status, buffer, sizeof(buffer));
    fprintf(stderr, "NT_ConfigOpen() failed: %s\n", buffer);
    return -1;
  }

  /* store the pid in a buffer for replacing our config ntpl statements */
  snprintf(pid_str, sizeof(pid_str), "%05X", (pid = getpid()));
  for (unsigned i = 0; i < sizeof(ntpl_common_config)/sizeof(ntpl_common_config[0]); ++i)
    ntpl_replace_xxxxx(ntpl_common_config[i], sizeof(ntpl_common_config[i]), pid_str);
  for (unsigned i = 0; i < sizeof(ntpl_config)/sizeof(ntpl_config[0]); ++i)
    ntpl_replace_xxxxx(ntpl_config[i], sizeof(ntpl_config[i]), pid_str);

  return 0;
}

static int ntpl(const char *str)
{
  int status;
  char buffer[NT_ERRBUF_SIZE];
  NtNtplInfo_t ntpl_info = {0};

  if ((status = NT_NTPL(config_stream, str, &ntpl_info, NT_NTPL_PARSER_VALIDATE_NORMAL)) != NT_SUCCESS) {
    // Get the status code as text
    NT_ExplainError(status, buffer, sizeof(buffer));
    fprintf(stderr, "NT_NTPL() failed: %s\n", buffer);
    fprintf(stderr, ">>> NTPL errorcode: %X\n", ntpl_info.u.errorData.errCode);
    fprintf(stderr, ">>> %s\n", ntpl_info.u.errorData.errBuffer[0]);
    fprintf(stderr, ">>> %s\n", ntpl_info.u.errorData.errBuffer[1]);
    fprintf(stderr, ">>> %s\n", ntpl_info.u.errorData.errBuffer[2]);
    return -1;
  }

  if (ntpl_info.ntplId)
    ntpl_ids[ntpl_ids_count++] = ntpl_info.ntplId;

  /* print out the ntpl so that the user can keep track */
  if (ntpl_verbose)
    printf("%d: %s\n", ntpl_info.ntplId, str);

  return 0;
}

static int ntpl_gen_streamids(char *out, size_t sz, uint32_t streams[], unsigned stream_count)
{
  int status, sz2 = 0;
  for (ssize_t i = 0; i < stream_count; ++i) {
    if ((size_t)(status = snprintf(out, sz,
                 "%s%d", i ? "," : "", streams[i])) >= sz) {
      sz2 += status;
      return sz2;
    }
    out += status;
    sz -= (size_t)status;
    sz2 += status;
  }
  return sz2;
}

static int ntpl_common_macros(void)
{
  char buffer[1024];
  int len;

  /* color macros; the only ones supported are the ones below,
   * currently in the nttun kernel driver: */
#define NTPL_COLOR(x,y) {#x,y}
  struct {
    const char *name;
    unsigned value;
  } colors[] = {
    NTPL_COLOR(COLOR_UNHANDLED_UPSTREAM, FLOW_COLOR_UNHANDLED),
    NTPL_COLOR(COLOR_UNHANDLED_DOWNSTREAM, FLOW_COLOR_UNHANDLED | FLOW_COLOR_DOWNSTREAM),
    NTPL_COLOR(COLOR_MISS_UPSTREAM, FLOW_COLOR_MISS),
    NTPL_COLOR(COLOR_MISS_DOWNSTREAM, FLOW_COLOR_DOWNSTREAM | FLOW_COLOR_MISS),
    NTPL_COLOR(COLOR_IPV6, FLOW_COLOR_IPV6),
    NTPL_COLOR(COLOR_TCP, FLOW_COLOR_TCP),
    NTPL_COLOR(COLOR_MONITOR, FLOW_COLOR_MONITOR),
    NTPL_COLOR(COLOR_OTHER, FLOW_COLOR_OTHER),
  };
#undef NTPL_COLOR

#define NTPL_CONST(x,y) {#x,y}
  struct {
    const char *name;
    unsigned value;
  } consts[] = {
    NTPL_CONST(KEYSET_ID_BYPASS, FLOW_KEYSET_ID_BYPASS),
    NTPL_CONST(KEYSET_ID_MONITOR, FLOW_KEYSET_ID_MONITOR),
    NTPL_CONST(KEY_ID_IPV4, FLOW_KEY_ID_IPV4),
    NTPL_CONST(KEY_ID_IPV6, FLOW_KEY_ID_IPV6),
  };
#undef NTPL_CONST

  for (unsigned i = 0; i < sizeof(colors)/sizeof(colors[0]); ++i ) {
    len = snprintf(buffer, sizeof(buffer),
                   "DefineMacro(\"" NTPL_PFX "_%05X_%s\", \"0x%02X\")",
                   pid, colors[i].name, colors[i].value);
    (void)len;
    assert((size_t)len < sizeof(buffer));
    if (ntpl(buffer) < 0)
      return -1;
  }

  for (unsigned i = 0; i < sizeof(consts)/sizeof(consts[0]); ++i ) {
    len = snprintf(buffer, sizeof(buffer),
                   "DefineMacro(\"" NTPL_PFX "_%05X_%s\", \"%u\")",
                   pid, consts[i].name, consts[i].value);
    (void)len;
    assert((size_t)len < sizeof(buffer));
    if (ntpl(buffer) < 0)
      return -1;
  }

  return 0;
}

int ntpl_common_apply(void)
{

#ifdef MEMDEBUG
  /* since flows persist between program instances, we need to delete=all for now
   * until figure a way of cleaning them up */
  if (ntpl("Delete=All") < 0)
    return -1;
#endif

  /* define macros that are used to set up our dynamic configuration */
  if (ntpl_common_macros() < 0)
    return -1;

  /* apply the filters dependent on those macro definitions (from common.ntpl) */
  for (unsigned i = 0; i < sizeof(ntpl_common_config)/sizeof(ntpl_common_config[0]); ++i)
    if (ntpl(ntpl_common_config[i]) < 0)
      return -1;

  return 0;
}


static int ntpl_macros(uint32_t streams[], unsigned stream_count, unsigned upstream, unsigned downstream)
{
  /* buffer has been calculated in the sense that our
   * streamids < 256 therefore, only 3 digits at most, plus 1 ','
   * or 1 final null, after each streamid = 4 * streams.
   * + 128 is > than length of any of the below macro strings.
   */
  char buffer[stream_count * 4 + 128];
  int len;

  /* DefineMacro("NTNETFLOW_XXXXXXX_STREAMIDS", "1,2,3,4") */
  /* Test type can either be NormalMode or CaptureMode */
  len = snprintf(buffer, sizeof(buffer),
                 "DefineMacro(\"" NTPL_PFX "_%05X_STREAMIDS\", \"", pid);

  len += ntpl_gen_streamids(buffer + len, sizeof(buffer) - (size_t)len, streams, stream_count);
  /* 2 bytes are appended below, 2 because snprintf does not count null in len,
   * so we overwrite the null provided by snprintf, and append one at the end. */
  assert((size_t)len + 2 < sizeof(buffer));
  buffer[len++] = '"';
  buffer[len++] = ')';
  buffer[len++] = '\0';
  if (ntpl(buffer) < 0)
    return -1;

  len = snprintf(buffer, sizeof(buffer),
                 "DefineMacro(\"" NTPL_PFX "_%05X_UPSTREAM_PORT\", \"%u\")",
                 pid, upstream);
  if (ntpl(buffer) < 0)
    return -1;

  len = snprintf(buffer, sizeof(buffer),
                 "DefineMacro(\"" NTPL_PFX "_%05X_DOWNSTREAM_PORT\", \"%u\")",
                 pid, downstream);
  if (ntpl(buffer) < 0)
    return -1;

  return 0;
}

/* default offsets needed: ,offset0=Layer3Header[0],offset1=Layer4Header[0] */
int ntpl_apply(uint32_t streams[], unsigned stream_count, unsigned upstream, unsigned downstream)
{
  /* define macros that are used to set up our dynamic configuration */
  if (ntpl_macros(streams, stream_count, upstream, downstream) < 0)
    return -1;

  /* apply the filters dependent on those macro definitions (from config.ntpl) */
  for (unsigned i = 0; i < sizeof(ntpl_config)/sizeof(ntpl_config[0]); ++i)
    if (ntpl(ntpl_config[i]) < 0)
      return -1;

  return 0;
}

void ntpl_deinit()
{
  int status;
  char buffer[MAX(64, NT_ERRBUF_SIZE)];

  for (int i = ntpl_ids_count - 1; i >= 0 && ntpl_ids[i]; --i) {
    int __attribute__((unused)) len = snprintf(buffer, sizeof(buffer), "Delete=%u", ntpl_ids[i]);
    assert((size_t)len < sizeof(buffer));
    ntpl(buffer);
  }

  if ((status = NT_ConfigClose(config_stream)) != NT_SUCCESS) {
    NT_ExplainError(status, buffer, sizeof(buffer));
    fprintf(stderr, "error: NT_ConfigClose() failed: %s\n", buffer);
  }
}

/* vim: set ts=2 sw=2 tw=2 expandtab : */
