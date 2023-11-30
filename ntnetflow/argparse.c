/**
 * Copyright (C) 2012-2015 Yecheng Fu <cofyc.jackson at gmail dot com>
 * All rights reserved.
 *
 * Use of this source code is governed by a MIT-style license that can be found
 * in the LICENSE file.
 *
 * Note: Some small changes has been made by Natatech.
 */

#include "argparse.h"

#define OPT_UNSET 1

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
argparse_error(struct argparse *This, const struct argparse_option *opt,
               const char *reason)
{
    if (!strncmp(This->argv[0], "--", 2)) {
        fprintf(stderr, "error: option `%s` %s\n", opt->long_name, reason);
        exit(-1);
    } else {
        fprintf(stderr, "error: option `%c` %s\n", opt->short_name, reason);
        exit(-1);
    }
}

static int
argparse_getvalue(struct argparse *This, const struct argparse_option *opt,
                  int flags)
{
    char *s;
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
        if (flags & OPT_UNSET) {
            *(int *)opt->value &= (int)~(1 << opt->data);
        } else {
            *(int *)opt->value |= (int)(1 << opt->data);
        }
        break;
    case ARGPARSE_OPT_BIT64:
      if (flags & OPT_UNSET) {
        *(uint64_t *)opt->value &= (uint64_t)~(1 << opt->data);
      } else {
        *(uint64_t *)opt->value |= (uint64_t)(1 << opt->data);
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
            argparse_error(This, opt, "requires a value");
        }
        break;
    case ARGPARSE_OPT_INTEGER:
        if (This->optvalue) {
            *(int *)opt->value = (int)strtol(This->optvalue, (char **)&s, 0);
            This->optvalue = NULL;
        } else if (This->argc > 1) {
            This->argc--;
            *(int *)opt->value = (int)strtol(*++This->argv, (char **)&s, 0);
        } else {
            argparse_error(This, opt, "requires a value");
            break;
        }
        if (*s)
            argparse_error(This, opt, "expects a numerical value");
        break;
    case ARGPARSE_OPT_UINT64:
      if (This->optvalue) {
#ifdef WIN32
        *(uint64_t *)opt->value = (uint64_t)_strtoui64(This->optvalue, (char **)&s, 0);
#else
        *(uint64_t *)opt->value = (uint64_t)strtoull(This->optvalue, (char **)&s, 0);
#endif
        This->optvalue = NULL;
      } else if (This->argc > 1) {
        This->argc--;
#ifdef WIN32
        *(uint64_t *)opt->value = (uint64_t)_strtoui64(*++This->argv, (char **)&s, 0);
#else
        *(uint64_t *)opt->value = (uint64_t)strtoull(*++This->argv, (char **)&s, 0);
#endif
      } else {
        argparse_error(This, opt, "requires a value");
        break;
      }
      if (*s)
        argparse_error(This, opt, "expects a numerical value");
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
            return argparse_getvalue(This, options, 0);
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
        return argparse_getvalue(This, options, opt_flags);
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
        fprintf(stderr, "error: unknown option `%s`\n", This->argv[0]);
        argparse_usage(This);
        exit(1);
    }

end:
    memmove((void *)(This->out + This->cpidx), This->argv,
            (size_t)This->argc * sizeof(*This->out));
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
        ssize_t pos;
        int pad;

        if (options->flags & ARGPARSE_INTERNAL_OPTION)
          continue;

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
        if ((size_t)pos <= usage_opts_width) {
            pad = (int)(usage_opts_width - (size_t)pos);
        } else {
            fputc('\n', stdout);
            pad = (int)usage_opts_width;
        }
#if 0
        fprintf(stdout, "%*s%s\n", pad + 2, "", options->help);
#else
        {
          size_t l = strlen(options->help);
          size_t i;
          fprintf(stdout, "%*s: ", pad, "");
          for (i = 0; i < l; i++)
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
