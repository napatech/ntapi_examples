/*
 * Copyright 2022 Napatech A/S. All rights reserved.
 *
 * 1. Copying, modification, and distribution of this file, or executable
 * versions of this file, is governed by the terms of the Napatech Software
 * license agreement under which this file was made available. If you do not
 * agree to the terms of the license do not install, copy, access or
 * otherwise use this file.
 *
 * 2. Under the Napatech Software license agreement you are granted a
 * limited, non-exclusive, non-assignable, copyright license to copy, modify
 * and distribute this file in conjunction with Napatech SmartNIC's and
 * similar hardware manufactured or supplied by Napatech A/S.
 *
 * 3. The full Napatech Software license agreement is included in this
 * distribution, please see "NP-0405 Napatech Software license
 * agreement.pdf"
 *
 * 4. Redistributions of source code must retain this copyright notice,
 * list of conditions and the following disclaimer.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" WITHOUT ANY WARRANTIES, EXPRESS OR
 * IMPLIED, AND NAPATECH DISCLAIMS ALL IMPLIED WARRANTIES INCLUDING ANY
 * IMPLIED WARRANTY OF TITLE, MERCHANTABILITY, NONINFRINGEMENT, OR OF
 * FITNESS FOR A PARTICULAR PURPOSE. TO THE EXTENT NOT PROHIBITED BY
 * APPLICABLE LAW, IN NO EVENT SHALL NAPATECH BE LIABLE FOR PERSONAL INJURY,
 * OR ANY INCIDENTAL, SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES WHATSOEVER,
 * INCLUDING, WITHOUT LIMITATION, DAMAGES FOR LOSS OF PROFITS, CORRUPTION OR
 * LOSS OF DATA, FAILURE TO TRANSMIT OR RECEIVE ANY DATA OR INFORMATION,
 * BUSINESS INTERRUPTION OR ANY OTHER COMMERCIAL DAMAGES OR LOSSES, ARISING
 * OUT OF OR RELATED TO YOUR USE OR INABILITY TO USE NAPATECH SOFTWARE OR
 * SERVICES OR ANY THIRD PARTY SOFTWARE OR APPLICATIONS IN CONJUNCTION WITH
 * THE NAPATECH SOFTWARE OR SERVICES, HOWEVER CAUSED, REGARDLESS OF THE THEORY
 * OF LIABILITY (CONTRACT, TORT OR OTHERWISE) AND EVEN IF NAPATECH HAS BEEN
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGES. SOME JURISDICTIONS DO NOT ALLOW
 * THE EXCLUSION OR LIMITATION OF LIABILITY FOR PERSONAL INJURY, OR OF
 * INCIDENTAL OR CONSEQUENTIAL DAMAGES, SO THIS LIMITATION MAY NOT APPLY TO YOU.
 */

#ifndef ARGPARSE_H
#define ARGPARSE_H
/**
 * Command-line arguments parsing library.
 *
 * This module is inspired by parse-options.c (git) and python's argparse
 * module.
 *
 * Arguments parsing is common task in cli program, but traditional `getopt`
 * libraries are not easy to use. This library provides high-level arguments
 * parsing solutions.
 *
 * The program defines what arguments it requires, and `argparse` will figure
 * out how to parse those out of `argc` and `argv`, it also automatically
 * generates help and usage messages and issues errors when users give the
 * program invalid arguments.
 *
 * Reserved namespaces:
 *  argparse
 *  OPT
 * Author: Yecheng Fu <cofyc.jackson@gmail.com>
 */

#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct argparse;
struct argparse_option;

typedef int argparse_callback(struct argparse *This, const struct argparse_option *option, int terminate);

enum argparse_flag {
    ARGPARSE_STOP_AT_NON_OPTION = 0x0001,
    ARGPARSE_INTERNAL_OPTION =    0x8000,
};

enum argparse_option_type {
    /* special */
    ARGPARSE_OPT_END,
    /* options with no arguments */
    ARGPARSE_OPT_BOOLEAN,
    ARGPARSE_OPT_BIT,
    ARGPARSE_OPT_BIT64,
    /* options with arguments (optional or required) */
    ARGPARSE_OPT_INTEGER,
    ARGPARSE_OPT_STRING,
    ARGPARSE_OPT_UINT64,
};

enum argparse_option_flags {
    OPT_NONEG = 1,              /* Negation disabled. */
};

/*
 *  Argparse option struct.
 *
 *  `type`:
 *    holds the type of the option, you must have an ARGPARSE_OPT_END last in your
 *    array.
 *
 *  `short_name`:
 *    the character to use as a short option name, '\0' if none.
 *
 *  `long_name`:
 *    the long option name, without the leading dash, NULL if none.
 *
 *  `value`:
 *    stores pointer to the value to be filled.
 *
 *  `help`:
 *    the short help message associated to what the option does.
 *    Must never be NULL (except for ARGPARSE_OPT_END).
 *
 *  `callback`:
 *    function is called when corresponding argument is parsed.
 *
 *  `data`:
 *    associated data. Callbacks can use it like they want.
 *
 *  `flags`:
 *    option flags.
 *
 */
struct argparse_option {
    enum argparse_option_type type;
    const char short_name;
    const char *long_name;
    void *value;
    const char *help;
    argparse_callback *callback;
    intptr_t data;
    int flags;
    const char *name_value;
};

/*
 * argpparse
 */
struct argparse {
    // user supplied
    const struct argparse_option *options;
    const char *const *usage;
    int flags;
    // internal context
    int argc;
    const char **argv;
    const char **out;
    int cpidx;
    const char *optvalue;       // current option value
};

// builtin option macros
#define OPT_END()          { ARGPARSE_OPT_END, '0', "", NULL, "", NULL, 0, 0, NULL }
#define OPT_BOOLEAN(...)   { ARGPARSE_OPT_BOOLEAN, __VA_ARGS__ }
#define OPT_BIT(...)       { ARGPARSE_OPT_BIT, __VA_ARGS__ }
#define OPT_BIT64(...)     { ARGPARSE_OPT_BIT64, __VA_ARGS__ }
#define OPT_INTEGER(...)   { ARGPARSE_OPT_INTEGER, __VA_ARGS__ }
#define OPT_UINT64(...)    { ARGPARSE_OPT_UINT64, __VA_ARGS__ }
#define OPT_STRING(...)    { ARGPARSE_OPT_STRING, __VA_ARGS__ }
#define OPT_HELP()         OPT_BOOLEAN('h', "help", NULL, "show this help message and exit", argparse_help_cb, 0, 0, NULL)

#ifdef __cplusplus
extern "C" {
#endif

// builtin callbacks
int argparse_help_cb(struct argparse *This, const struct argparse_option *option, int terminate);

int argparse_init(struct argparse *This, struct argparse_option *options,
                  const char *const *usage, int flags);
int argparse_parse(struct argparse *This, int argc, const char **argv);
void argparse_usage(struct argparse *This);

#ifdef __cplusplus
}
#endif

#endif
