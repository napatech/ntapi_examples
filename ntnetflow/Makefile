include config.mk

.SUFFIXES:
.SUFFIXES: .o .c

HDR =\
	argparse.h\
	common.h\
	flowtable.h\
	flowinfo.h\
	flowlearn.h\
	flowkey.h\
	flowstats.h\
	futex.h\
	info.h\
	mpmc_queue.h\
	ntpl.h\
	ntpl_common_config.h\
	ntpl_config.h\
	rx.h\
	signal.h\
	stats.h

BIN = ntnetflow

LIBUTIL = libutil.a
LIBUTILSRC =\
	argparse.c

LIBNTNETFLOW = libntnetflow.a
LIBNTNETFLOWSRC =\
	common.c\
	flowinfo.c\
	flowtable.c\
	flowlearn.c\
	flowstats.c\
	info.c\
	ntpl.c\
	rx.c\
	signal.c\
	stats.c

LIB = $(LIBNTNETFLOW) $(LIBUTIL)

LIBUTILOBJ = $(LIBUTILSRC:.c=.o)
LIBNTNETFLOWOBJ = $(LIBNTNETFLOWSRC:.c=.o)

OBJ = $(BIN:=.o) $(LIBUTILOBJ) $(LIBNTNETFLOWOBJ)
SRC = $(BIN:=.c)

all: $(BIN)

$(BIN): $(LIB) $(@:=.o)

$(OBJ): $(HDR) config.mk

.o:
	$(CC) $(LDFLAGS) -o $@ $< $(LIB)

.c.o:
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ -c $<

ntpl_common_config.h: common.ntpl
	sed -n -e '1i/*auto-generated from $<, do not edit*/\nstatic char ntpl_common_config[][512] = {' -e '$$a};' -e 's/#.*//g' -e 's/"/\\"/g' -e '/^[[:space:]]*$$/!s/\(.*\)/  "\1",/p' $< > $@

ntpl_config.h: config.ntpl
	sed -n -e '1i/*auto-generated from $<, do not edit*/\nstatic char ntpl_config[][512] = {' -e '$$a};' -e 's/#.*//g' -e 's/"/\\"/g' -e '/^[[:space:]]*$$/!s/\(.*\)/  "\1",/p' $< > $@

$(LIBUTIL): $(LIBUTILOBJ)
	$(AR) rc $@ $?
	$(RANLIB) $@

$(LIBNTNETFLOW): $(LIBNTNETFLOWOBJ)
	$(AR) rc $@ $?
	$(RANLIB) $@

tags:
	ctags -R . /opt/napatech3/include/ntapi

.gitignore:
	printf "%s\n" tags 'core.*' $(OBJ) $(BIN) $(LIB) > .gitignore

clean:
	rm -f $(OBJ) $(BIN) $(LIB) ntpl_common_config.h ntpl_config.h tags

.PHONY: all clean tags .gitignore
