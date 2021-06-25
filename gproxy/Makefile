SHELL = /bin/sh
SYSTEM = $(shell uname)
C++ = i686-w64-mingw32-g++
CC = i686-w64-mingw32-gcc
DFLAGS =
OFLAGS = -m32 -O2 -march=i486 -mtune=generic -flto
LFLAGS = -static -static-libgcc -static-libstdc++ -L. -lwinmm -lwsock32
CFLAGS = -w

CFLAGS += $(OFLAGS) $(DFLAGS) -I.

OBJS = commandpacket.o config.o gameprotocol.o gproxy.o gpsprotocol.o socket.o util.o incominggamehost.o
COBJS =
PROGS = dods.mix

all: $(OBJS) $(COBJS) $(PROGS)

dods.mix: $(OBJS) $(COBJS)
	$(C++) -shared -o dods.mix $(OBJS) $(COBJS) $(LFLAGS)

clear:
	rm -f $(OBJS) $(COBJS) $(PROGS)

$(OBJS): %.o: %.cpp
	$(C++) -o $@ $(CFLAGS) -c $<

$(COBJS): %.o: %.c
	$(CC) -o $@ $(CFLAGS) -c $<

all: clear; strip dods.mix; mv dods.mix ../dods.mix

commandpacket.o: gproxy.h commandpacket.h
config.o: gproxy.h config.h
gameprotocol.o: gproxy.h util.h gameprotocol.h
incominggamehost.o: gproxy.h util.h incominggamehost.h
gproxy.o: gproxy.h util.h config.h socket.h commandpacket.h gameprotocol.h gpsprotocol.h incominggamehost.h
gpsprotocol.o: gproxy.h util.h gpsprotocol.h
socket.o: gproxy.h util.h socket.h
util.o: gproxy.h util.h