#
# Copyright 2019 Napatech A/S. All rights reserved.
# 
# 1. Copying, modification, and distribution of this file, or executable
# versions of this file, is governed by the terms of the Napatech Software
# license agreement under which this file was made available. If you do not
# agree to the terms of the license do not install, copy, access or
# otherwise use this file.
# 
# 2. Under the Napatech Software license agreement you are granted a
# limited, non-exclusive, non-assignable, copyright license to copy, modify
# and distribute this file in conjunction with Napatech SmartNIC's and
# similar hardware manufactured or supplied by Napatech A/S.
# 
# 3. The full Napatech Software license agreement is included in this
# distribution, please see "NP-0405 Napatech Software license
# agreement.pdf"
# 
# 4. Redistributions of source code must retain this copyright notice,
# list of conditions and the following disclaimer.
# 
# THIS SOFTWARE IS PROVIDED "AS IS" WITHOUT ANY WARRANTIES, EXPRESS OR
# IMPLIED, AND NAPATECH DISCLAIMS ALL IMPLIED WARRANTIES INCLUDING ANY
# IMPLIED WARRANTY OF TITLE, MERCHANTABILITY, NONINFRINGEMENT, OR OF
# FITNESS FOR A PARTICULAR PURPOSE. TO THE EXTENT NOT PROHIBITED BY
# APPLICABLE LAW, IN NO EVENT SHALL NAPATECH BE LIABLE FOR PERSONAL INJURY,
# OR ANY INCIDENTAL, SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES WHATSOEVER,
# INCLUDING, WITHOUT LIMITATION, DAMAGES FOR LOSS OF PROFITS, CORRUPTION OR
# LOSS OF DATA, FAILURE TO TRANSMIT OR RECEIVE ANY DATA OR INFORMATION,
# BUSINESS INTERRUPTION OR ANY OTHER COMMERCIAL DAMAGES OR LOSSES, ARISING
# OUT OF OR RELATED TO YOUR USE OR INABILITY TO USE NAPATECH SOFTWARE OR
# SERVICES OR ANY THIRD PARTY SOFTWARE OR APPLICATIONS IN CONJUNCTION WITH
# THE NAPATECH SOFTWARE OR SERVICES, HOWEVER CAUSED, REGARDLESS OF THE THEORY
# OF LIABILITY (CONTRACT, TORT OR OTHERWISE) AND EVEN IF NAPATECH HAS BEEN
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGES. SOME JURISDICTIONS DO NOT ALLOW
# THE EXCLUSION OR LIMITATION OF LIABILITY FOR PERSONAL INJURY, OR OF
# INCIDENTAL OR CONSEQUENTIAL DAMAGES, SO THIS LIMITATION MAY NOT APPLY TO YOU.
#
# Top level makefile for tools
#

# Set default locale for commands like tr, bash and others.
LC_ALL = C

# Use the bash shell for all Makefile shell commands
# bash shell is required because of the use of PIPESTATUS[0] in color-command macro function.
SHELL := /usr/bin/env LC_ALL=C bash

# Platform/Architecture
PLATFORM := $(shell uname -s | tr "[A-Z]" "[a-z]")
SUBARCH  := $(shell uname -m)

# Get gcc version
cc-version = $(shell $(CC) -v 2>&1 | grep "gcc version" | cut -d ' ' -f 3 | tr -d .)
cc-ifversion = $(shell test $(call cc-version) $(1) $(2) && echo $(3))

# We need at least GCC 4.0
ifneq (1, $(call cc-ifversion, -ge, 400, 1))
$(error Only GCC 4.x is supported)
endif

# Add version information
CFLAGS += -D'TOOLS_RELEASE_VERSION="0.0.0.ADVEX"' -D'RELEASE_TAG="ADVEX"'

# Produce debugging information
CFLAGS += -g

# Optimize
CFLAGS += -O3

# Source compliance
CFLAGS += -std=gnu99

# Add warning flags
CFLAGS += -Wall -Wextra -Wstrict-prototypes -Wmissing-prototypes -Wcast-qual -Winline -Wno-error -Wsign-compare -Wunused-parameter -Wunused-function

# Platform / Architecture specific CFLAGS
# For gcc < 4.3 use nocona otherwise use core2 optimization
# core2 tuning was officially introduced in gcc 4.3
ifeq ($(findstring 64,$(SUBARCH)),64)
CFLAGS  += -D_NT_HOST_64BIT
ifeq (1, $(call cc-ifversion, -lt, 430, 1))
CFLAGS  += -march=nocona
else
CFLAGS  += -march=core2
endif
else
CFLAGS  += -D_NT_HOST_32BIT
CFLAGS  += -march=prescott
# Build linux 32bit with large file support on all sys calls
ifeq ($(findstring linux,$(PLATFORM)),linux)
CFLAGS  += -D_FILE_OFFSET_BITS=64
endif
endif

# Add include path
CFLAGS += -I/opt/napatech3 -I/opt/napatech3/include -I. -I.. -I./shared

# Linker flags
LDFLAGS += -L/opt/napatech3/lib ./shared/shared.a -lntapi -lntos -lm
LDFLAGS += -lpthread

# Object files to build
OBJS := $(notdir $(SRCS:%.c=%.o))

# Common targets
.PHONY: all
all: build

.PHONY: build
build: $(TARGET)

# Add this Makefile to the dependencies
$(OBJS): Makefile

$(OBJS): %.o: %.c

$(TARGET): $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $@

%.o: %.c
	$(CC) -c $(CFLAGS) -o $(@) $(realpath $(<))

.PHONY: clean
clean:
	@rm -f $(TARGET)
	@rm -f $(OBJS)
	@rm -f buildflags
	@rm -f *.deps
