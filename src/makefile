CC      = gcc
CFLAGS  = -O1 -Wall -fcommon
INCDIR  = -I/usr/include/json-c -I/usr/include/microhttpd -I/usr/include/openssl -I/usr/include/systemd/sd-daemon
DESTDIR = /usr/local/hhl7
BINDIR  = /usr/local/bin
MANDIR  = /usr/share/man
LIBDIR  = 
LIBS    = -lm -ljson-c -lmicrohttpd -lssl -lcrypto -lsystemd
OBJS    = hhl7webpages.o hhl7web.o hhl7auth.o hhl7net.o hhl7utils.o hhl7json.o hhl7.o
BIN     = hhl7
MAN     = hhl7.1
CERTS   = certs/*.example
IMAGES  = images/*
SVRS    = conf/servers.hhl7
SYSD    = sys/hhl7.s*
SYSLOG  = sys/76-hhl7.conf
TEMPS   = templates/*

.c.o:
	$(CC) $(CFLAGS) -D$(shell echo `uname -s`) -c $< -o $*.o $(INCDIR)

all:	hhl7.o hhl7

hhl7:	$(OBJS) 
	$(CC) $(COPTS) $(SYSTEM) -o $(BIN) $^ $(INCDIR) $(LIBDIR) $(LIBS)

clean:
	for i in $(OBJS) ; do \
		rm -f $$i; \
	done
	rm -f $(BIN)

install:: hhl7
	install -c -d -m 0755 $(DESTDIR)/bin
	install -c -d -m 0755 $(DESTDIR)/certs
	install -c -d -m 0755 $(DESTDIR)/conf
	install -c -d -m 0755 $(DESTDIR)/images
	install -c -d -m 0755 $(DESTDIR)/templates
	install -c -s -m 0755 $(BIN) $(DESTDIR)/bin
	ln -sf $(DESTDIR)/bin/$(BIN) $(BINDIR)/$(BIN)
	install -c -m 0644 $(CERTS) $(DESTDIR)/certs
	install -c -m 0644 $(IMAGES) $(DESTDIR)/images
	install -c -m 0644 $(SVRS) $(DESTDIR)/$(SVRS).example
	install -c -m 0644 $(SYSD) /lib/systemd/system
	install -c -m 0644 $(SYSLOG) /etc/rsyslog.d
	install -c -m 0644 $(TEMPS) $(DESTDIR)/templates

#	TODO - install man page when created
#	install -c -m 0644 $(MAN) $(MANDIR)/man1 
