# $Id: Makefile 14837 2011-10-11 15:52:35Z uj $

# Set the target name
TARGET=NT2JSON

SRCS = $(TARGET).c

INSTALL_PATH?=/opt/napatech3


INC_DIR:= $(INSTALL_PATH)/include
LIB_DIR:= $(INSTALL_PATH)/lib


OBJS := $(SRCS:%.c=%.o)
DEPS := $(SRCS:%.c=%.deps)

# No specific extended descriptor has been selected and support for
# all extended descriptor types has been enabled. This will not
# utilize optimal performance in applications. Use e.g.
# _NTAPI_EXTDESCR_7_ if ntservice.ini has PacketDescriptor=Ext7.
DEFINES += -D_NTAPI_EXTDESCR_ALL_

# Set the include path
IFLAGS += -I$(INC_DIR)

# Require the ntapi lib
LDFLAGS += -L$(LIB_DIR) -L./../lib -lntapi -lstdc++ -lpthread -ljson-c

# Include user specified CFLAGS
CFLAGS = $(USER_FLAGS)

# Check for 32/64bit to enable optimal GCC architecture
SUBARCH := $(shell uname -m)
ifeq ($(findstring 64,$(SUBARCH)),64)
CFLAGS += -march=nocona
else
CFLAGS += -march=prescott
endif

CFLAGS += -O2 $(IFLAGS) $(DEFINES) -g
CFLAGS += -Wall -Wextra -Wstrict-prototypes -Wmissing-prototypes -Wcast-qual -Winline -Wsign-compare -Wunused-parameter -Wpointer-arith -Wshadow

#
#
#

.PHONY: all
all: build

.PHONY: build
build: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(^) $(LDFLAGS) -o $(@)

# Add this Makefile to the dependencies
$(OBJS): Makefile

.PHONY: clean
clean:
	@rm -f $(TARGET)
	@rm -f $(OBJS)
	@rm -f $(DEPS)
	@rm -f $(OBJS).cflags

install: all
