CC = gcc
AR = ar
RANLIB = ranlib

CPPFLAGS = -I/opt/napatech3/include -D_DEFAULT_SOURCE -D_BSD_SOURCE -D_XOPEN_SOURCE=700 -DNDEBUG
CFLAGS   = -std=gnu11 -Wall -pthread -O3
LDFLAGS  = -s -L/opt/napatech3/lib -lntapi -lm -lrt -pthread
