SHELL = /bin/sh
SYSTEM = $(shell uname)
C++ = i686-w64-mingw32-g++
CC = i686-w64-mingw32-gcc
DFLAGS =
OFLAGS = -O2
LFLAGS = -static -static-libgcc -static-libstdc++ -L. -lwinmm -lwsock32 -lgmp.dll -lssp
CFLAGS =

ifeq ($(SYSTEM),Darwin)
DFLAGS += -D__APPLE__
OFLAGS += -flat_namespace
endif

ifeq ($(SYSTEM),FreeBSD)
DFLAGS += -D__FREEBSD__
endif

ifeq ($(SYSTEM),SunOS)
DFLAGS += -D__SOLARIS__
LFLAGS += -lresolv -lsocket -lnsl
endif

CFLAGS += $(OFLAGS) $(DFLAGS) -I.

OBJS = commandpacket.o config.o gameprotocol.o gproxy.o gpsprotocol.o socket.o util.o incominggamehost.o
COBJS =
PROGS = ./gproxy++

all: $(OBJS) $(COBJS) $(PROGS)

./gproxy++: $(OBJS) $(COBJS)
	$(C++) -o ./gproxy++ $(OBJS) $(COBJS) $(LFLAGS)

clean:
	rm -f $(OBJS) $(COBJS) $(PROGS)

$(OBJS): %.o: %.cpp
	$(C++) -o $@ $(CFLAGS) -c $<

$(COBJS): %.o: %.c
	$(CC) -o $@ $(CFLAGS) -c $<

./gproxy++: $(OBJS) $(COBJS)

all: $(PROGS)

commandpacket.o: gproxy.h commandpacket.h
config.o: gproxy.h config.h
gameprotocol.o: gproxy.h util.h gameprotocol.h
incominggamehost.o: gproxy.h util.h incominggamehost.h
gproxy.o: gproxy.h util.h config.h socket.h commandpacket.h gameprotocol.h gpsprotocol.h incominggamehost.h
gpsprotocol.o: gproxy.h util.h gpsprotocol.h
socket.o: gproxy.h util.h socket.h
util.o: gproxy.h util.h
