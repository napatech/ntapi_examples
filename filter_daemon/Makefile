TARGET := ntfilterd

SRCS = ntfilterd.cpp NTFileHandler.cpp IPAddress.cpp NTUtils.cpp
	

INSTALL_PATH?=/opt/napatech3


INC_DIR:= $(INSTALL_PATH)/include
LIB_DIR:= $(INSTALL_PATH)/lib

XML_INC:= /usr/include/libxml2
XML_LIB:= /usr/lib
#
# Put common example definitions here
#

OBJS := $(SRCS:%.cpp=%.o)
DEPS := $(SRCS:%.cpp=%.deps)

# No specific extended descriptor has been selected and support for
# all extended descriptor types has been enabled. This will not
# utilize optimal performance in applications. Use e.g.
# _NTAPI_EXTDESCR_7_ if ntservice.ini has PacketDescriptor=Ext7.
DEFINES += -D_NTAPI_EXTDESCR_ALL_

# Set the include path
IFLAGS += -I$(XML_INC)
IFLAGS += -I$(INC_DIR)


# Require the ntapi lib
LDFLAGS += -L$(LIB_DIR) -L$(XML_LIB) -L./../lib -lntapi  -lstdc++ -lpthread 

# Include user specified CFLAGS
CXXFLAGS = $(USER_FLAGS)

# Check for 32/64bit to enable optimal GCC architecture
SUBARCH := $(shell uname -m)
ifeq ($(findstring 64,$(SUBARCH)),64)
CXXFLAGS += -march=nocona
else
CXXFLAGS += -march=prescott
endif

CXXFLAGS += -O2 $(IFLAGS) $(DEFINES) -g
CXXFLAGS += -Wall -Wextra -Wstrict-prototypes -Wmissing-prototypes -Wcast-qual -Winline -Wsign-compare -Wunused-parameter -Wpointer-arith -Wshadow

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
#	@rm -f $(OBJS)
#	@rm -f $(DEPS)
	@rm -f $(OBJS).cflags

install: all

print_vars:
	echo $(CFLAGS)



