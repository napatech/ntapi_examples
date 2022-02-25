#ifdef WIN32
#include <winsock2.h>
#endif
#include "argparse.h"
#include <errno.h>
#include <limits.h>

#include "../tool_common.h"

#define OPT_UNSET 1

#define OPTION_SHORT 0
#define OPTION_LONG  1

static const char *
prefix_skip(const char *str, const char *prefix)
{
    size_t len = strlen(prefix);
    return strncmp(str, prefix, len) ? NULL : str + len;
}

static int
prefix_cmp(const char *str, const char *prefix)
{
    for (;; str++, prefix++)
        if (!*prefix)
            return 0;
        else if (*str != *prefix)
            return (unsigned char)*prefix - (unsigned char)*str;
}

static void
argparse_error(const struct argparse_option* opt,
               const char*                   reason,
                     int                     optLenLong)
{
    if (optLenLong) {
        fprintf(stderr, ">>> Error: option `%s` %s\n", opt->long_name, reason);
    } else {
        fprintf(stderr, ">>> Error: option `%c` %s\n", opt->short_name, reason);
    }

    exit(NT_APPL_ERROR_OPT_ARG);
}

static int
argparse_getvalue(struct argparse *This, const struct argparse_option *opt,
                  int flags, int optLenLong)
{
    char     *s;
    long int long_int;
    char     errMsg[128];

    if (!opt->value)
        goto skipped;
    switch (opt->type) {
    case ARGPARSE_OPT_BOOLEAN:
        if (flags & OPT_UNSET) {
            *(int *)opt->value = *(int *)opt->value - 1;
        } else {
            *(int *)opt->value = *(int *)opt->value + 1;
        }
        if (*(int *)opt->value < 0) {
            *(int *)opt->value = 0;
        }
        break;
    case ARGPARSE_OPT_BIT:
      if (This->optvalue != NULL) {
        argparse_error(opt, "value not allowed", optLenLong);
      }
      if (flags & OPT_UNSET) {
        *(int *)opt->value &= (int)~(1 << opt->data);
      } else {
        *(int *)opt->value |= (int)(1 << opt->data);
      }
      break;
    case ARGPARSE_OPT_BIT64:
      if (This->optvalue != NULL) {
        argparse_error(opt, "value not allowed", optLenLong);
      }
      if (flags & OPT_UNSET) {
        *(uint64_t *)opt->value &= ~(1ULL << opt->data);
      } else {
        *(uint64_t *)opt->value |= 1ULL << opt->data;
      }
      break;
    case ARGPARSE_OPT_STRING:
        if (This->optvalue) {
            *(const char **)opt->value = This->optvalue;
            This->optvalue = NULL;
        } else if (This->argc > 1) {
            This->argc--;
            *(const char **)opt->value = *++This->argv;
        } else {
            argparse_error(opt, "requires a value", optLenLong);
        }
        break;
    case ARGPARSE_OPT_INTEGER:
        errno = 0;

        if (This->optvalue) {
            long_int = strtol(This->optvalue, &s, 0);
            This->optvalue = NULL;
        } else if (This->argc > 1) {
            This->argc--;
            long_int = strtol(*++This->argv, &s, 0);
        } else {
            argparse_error(opt, "requires a value", optLenLong);
            break;
        }

        //Test if strtol issued a range error or that the output from strtol
        //cannot be applied to a 32bit int
        if (errno == ERANGE
#if defined(LONG_MAX) && defined(INT_MAX) && (LONG_MAX > INT_MAX)
            || long_int > INT_MAX || long_int < INT_MIN
#endif
          )
        {
          (void)snprintf(errMsg, sizeof(errMsg), "value out of range [%d..%d]",
                         INT_MIN, INT_MAX);
          argparse_error(opt, errMsg, optLenLong);
          break;
        }

        if (*s)
        {
          argparse_error(opt, "expects a numerical value", optLenLong);
          break;
        }

        *(int *)opt->value = (int)long_int;
        break;
    case ARGPARSE_OPT_UINT64:
      if (This->optvalue) {
#ifdef WIN32
        *(uint64_t *)opt->value = _strtoui64(This->optvalue, &s, 0);
#else
        errno = 0;
        *(uint64_t *)opt->value = strtoull(This->optvalue, &s, 0);
#endif
        This->optvalue = NULL;
      } else if (This->argc > 1) {
        This->argc--;
#ifdef WIN32
        *(uint64_t *)opt->value = _strtoui64(*++This->argv, &s, 0);
#else
        errno = 0;
        *(uint64_t *)opt->value = strtoull(*++This->argv, &s, 0);
#endif
      } else {
        argparse_error(opt, "requires a value", optLenLong);
        break;
      }
      if (*s)
        argparse_error(opt, "expects a numerical value", optLenLong);
#if !defined(WIN32)
      if (errno == ERANGE) {
        char msgbuf[256];
        (void)snprintf(msgbuf, sizeof(msgbuf), "is too large");
        argparse_error(opt, msgbuf, optLenLong);
      }
#endif
      break;
    default:
        assert(0);
    }

skipped:
    if (opt->callback) {
        return opt->callback(This, opt, 1);
    }

    return 0;
}

static void
argparse_options_check(const struct argparse_option *options)
{
    for (; options->type != ARGPARSE_OPT_END; options++) {
        switch (options->type) {
        case ARGPARSE_OPT_END:
        case ARGPARSE_OPT_BOOLEAN:
        case ARGPARSE_OPT_BIT:
        case ARGPARSE_OPT_BIT64:
        case ARGPARSE_OPT_INTEGER:
        case ARGPARSE_OPT_UINT64:
        case ARGPARSE_OPT_STRING:
            continue;
        default:
            fprintf(stderr, "wrong option type: %d", options->type);
            break;
        }
    }
}

static int
argparse_short_opt(struct argparse *This, const struct argparse_option *options)
{
    for (; options->type != ARGPARSE_OPT_END; options++) {
        if (options->short_name == *This->optvalue) {
            This->optvalue = This->optvalue[1] ? This->optvalue + 1 : NULL;
            return argparse_getvalue(This, options, 0, OPTION_SHORT);
        }
    }
    return -2;
}

static int
argparse_long_opt(struct argparse *This, const struct argparse_option *options)
{
    for (; options->type != ARGPARSE_OPT_END; options++) {
        const char *rest;
        int opt_flags = 0;
        if (!options->long_name)
            continue;

        rest = prefix_skip(This->argv[0] + 2, options->long_name);
        if (!rest) {
            // Negation allowed?
            if (options->flags & OPT_NONEG) {
                continue;
            }
            // Only boolean/bit allow negation.
            if (options->type != ARGPARSE_OPT_BOOLEAN && options->type != ARGPARSE_OPT_BIT && options->type != ARGPARSE_OPT_BIT64) {
                continue;
            }

            if (!prefix_cmp(This->argv[0] + 2, "no-")) {
                rest = prefix_skip(This->argv[0] + 2 + 3, options->long_name);
                if (!rest)
                    continue;
                opt_flags |= OPT_UNSET;
            } else {
                continue;
            }
        }
        if (*rest) {
            if (*rest != '=')
                continue;
            This->optvalue = rest + 1;
        }
        return argparse_getvalue(This, options, opt_flags, OPTION_LONG);
    }
    return -2;
}

int
argparse_init(struct argparse *This, struct argparse_option *options,
              const char *const *usage, int flags)
{
    memset(This, 0, sizeof(*This));
    This->options = options;
    This->usage = usage;
    This->flags = flags;
    return 0;
}

int
argparse_parse(struct argparse *This, int argc, const char **argv)
{
    This->argc = argc - 1;
    This->argv = argv + 1;
    This->out = argv;

    argparse_options_check(This->options);

    for (; This->argc; This->argc--, This->argv++) {
        const char *arg = This->argv[0];
        if (arg[0] != '-' || !arg[1]) {
            if (This->flags & ARGPARSE_STOP_AT_NON_OPTION) {
                goto end;
            }
            // if it's not option or is a single char '-', copy verbatimly
            This->out[This->cpidx++] = This->argv[0];
            continue;
        }
        // short option
        if (arg[1] != '-') {
            This->optvalue = arg + 1;
            switch (argparse_short_opt(This, This->options)) {
            case -1:
                break;
            case -2:
                goto unknown;
            }
            while (This->optvalue) {
                switch (argparse_short_opt(This, This->options)) {
                case -1:
                    break;
                case -2:
                    goto unknown;
                }
            }
            continue;
        }
        // if '--' presents
        if (!arg[2]) {
            This->argc--;
            This->argv++;
            break;
        }
        // long option
        switch (argparse_long_opt(This, This->options)) {
        case -1:
            break;
        case -2:
            goto unknown;
        }
        continue;

unknown:
        fprintf(stderr, ">>> Error: unknown option `%s`\n", This->argv[0]);
        argparse_usage(This);
        exit(NT_APPL_ERROR_OPT_ILL);
    }

end:
    memmove(This->out + This->cpidx, This->argv,
            This->argc * sizeof(*This->out));
    This->out[This->cpidx + This->argc] = NULL;

    return This->cpidx + This->argc;
}

void
argparse_usage(struct argparse *This)
{
    const struct argparse_option *options;

    // figure out best width
    size_t usage_opts_width = 0;
    size_t len;

    fprintf(stdout, "%s\n", *This->usage++);

    options = This->options;
    for (; options->type != ARGPARSE_OPT_END; options++) {
        len = 0;
        if ((options)->short_name) {
            len += 2;
        }
        if ((options)->short_name && (options)->long_name) {
            len += 2;           // separator ", "
        }
        if ((options)->long_name) {
            len += strlen((options)->long_name) + 2;
        }
        if (options->name_value != NULL) {
          len += strlen(options->name_value) + 3;
        }
        else {
          if (options->type == ARGPARSE_OPT_INTEGER) {
            len += strlen(" <int>");
          } else if (options->type == ARGPARSE_OPT_UINT64) {
            len += strlen(" <int64>");
          } else if (options->type == ARGPARSE_OPT_STRING) {
            len += strlen(" <str>");
          }
        }
        len = (size_t)ceil((float)len / 4) * 4;
        if (usage_opts_width < len) {
            usage_opts_width = len;
        }
    }
    usage_opts_width += 4;      // 4 spaces prefix

    options = This->options;
    for (; options->type != ARGPARSE_OPT_END; options++) {
        size_t pos;
        int pad;

#if !defined(DEBUG)
        if (options->flags & ARGPARSE_INTERNAL_OPTION) {
          continue;
        }
#endif

        pos = fprintf(stdout, "    ");
        if (options->short_name) {
            pos += fprintf(stdout, "-%c", options->short_name);
        }
        if (options->long_name && options->short_name) {
            pos += fprintf(stdout, ", ");
        }
        if (options->long_name) {
            pos += fprintf(stdout, "--%s", options->long_name);
        }
        if (options->name_value != NULL) {
          pos += fprintf(stdout, " <%s>", options->name_value);
        }
        else {
          if (options->type == ARGPARSE_OPT_INTEGER) {
            pos += fprintf(stdout, " <int>");
          } else if (options->type == ARGPARSE_OPT_UINT64) {
              pos += fprintf(stdout, " <int64>");
          } else if (options->type == ARGPARSE_OPT_STRING) {
              pos += fprintf(stdout, " <str>");
          }
        }
        if (pos <= usage_opts_width) {
            pad = (int)(usage_opts_width - pos);
        } else {
            fputc('\n', stdout);
            pad = (int)usage_opts_width;
        }
#if 0
        fprintf(stdout, "%*s%s\n", pad + 2, "", options->help);
#else
        {
          size_t len = strlen(options->help);
          size_t i;
          fprintf(stdout, "%*s: ", pad, "");

#if defined(DEBUG)
          if (options->flags & ARGPARSE_INTERNAL_OPTION) {
            fprintf(stdout, "NOTE: Napatech internal option\n");
            fprintf(stdout, "%*s", (int)(usage_opts_width + 2), "");
          }
#endif

          for (i = 0; i < len; i++)
          {
            fputc(options->help[i], stdout);
            if (options->help[i] == '\n') {
              /* A newline in the text. We must indent the next text */
              fprintf(stdout, "%*s", (int)(usage_opts_width + 2), "");
            }
          }
          fputc('\n', stdout);
        }
#endif
    }
    while (*This->usage && **This->usage)
      fprintf(stdout, "%s\n", *This->usage++);
    fputc('\n', stdout);
}

int
argparse_help_cb(struct argparse *This, const struct argparse_option *option, int terminate)
{
    (void)option;
    argparse_usage(This);
    if (terminate == 1)
      exit(0);
    return 0;
}
