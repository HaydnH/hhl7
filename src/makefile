CC       = gcc
CFLAGS   = -O3 -Wall   # Live
#CFLAGS   = -Wall -Wextra -Wno-overlength-strings -Wpedantic -g3  # Debug
#CFLAGS   = -O3 -Wall -Wextra -Wno-overlength-strings -Wpedantic -g3  # Devel & Debug
#CFLAGS   = -O3 -Wall -Wextra -fsanitize=undefined,address -g3        # UBSan/Asan
INCDIR   = -I/usr/include/json-c -I/usr/include/microhttpd -I/usr/include/openssl -I/usr/include/systemd/sd-daemon

HHL7USER = hhl7
DESTDIR  = /usr/local/hhl7
BINDIR   = /usr/local/bin
MANDIR   = /usr/share/man/man1
LIBDIR   = 
LIBS     = -lm -ljson-c -lmicrohttpd -lssl -lcrypto -lsystemd
#LIBS     = -lasan -lm -ljson-c -lmicrohttpd -lssl -lcrypto -lsystemd -lubsan   # UBSan
OBJS     = hhl7webpages.o hhl7web.o hhl7auth.o hhl7net.o hhl7utils.o hhl7json.o hhl7.o
BIN      = hhl7
MAN      = man/hhl7.1
CERTS    = certs/*.example
IMAGES   = images/*
LOGROT   = sys/hhl7.logrotate
SVRS     = conf/servers.hhl7
SYSD     = sys/hhl7.s*
SYSLOG   = sys/76-hhl7.conf
BASHCP   = sys/hhl7.bashComplete
RESPS    = responders/*
TEMPS    = templates/*
DATAS    = datafiles/*

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
	install -o $(HHL7USER) -g $(HHL7USER) -c -d -m 0755 $(DESTDIR)/bin
	install -o $(HHL7USER) -g $(HHL7USER) -c -d -m 0755 $(DESTDIR)/certs
	install -o $(HHL7USER) -g $(HHL7USER) -c -d -m 0755 $(DESTDIR)/conf
	install -o $(HHL7USER) -g $(HHL7USER) -c -d -m 0755 $(DESTDIR)/images
	install -o $(HHL7USER) -g $(HHL7USER) -c -d -m 0755 $(DESTDIR)/responders
	install -o $(HHL7USER) -g $(HHL7USER) -c -d -m 0755 $(DESTDIR)/templates
	install -o $(HHL7USER) -g $(HHL7USER) -c -d -m 0755 $(DESTDIR)/datafiles
	install -o $(HHL7USER) -g $(HHL7USER) -c -s -m 0755 $(BIN) $(DESTDIR)/bin
	ln -sf $(DESTDIR)/bin/$(BIN) $(BINDIR)/$(BIN)
	install -o $(HHL7USER) -g $(HHL7USER) -c -m 0644 $(CERTS) $(DESTDIR)/certs
	install -o $(HHL7USER) -g $(HHL7USER) -c -m 0644 $(IMAGES) $(DESTDIR)/images
	install -c -m 0644 $(LOGROT) /etc/logrotate.d/$(BIN)
	install -o $(HHL7USER) -g $(HHL7USER) -c -m 0644 $(SVRS) $(DESTDIR)/$(SVRS).example
	install -c -m 0644 $(SYSD) /lib/systemd/system
	install -c -m 0644 $(SYSLOG) /etc/rsyslog.d
	install -c -m 0644 $(BASHCP) /usr/share/bash-completion/completions/$(BIN)
	install -o $(HHL7USER) -g $(HHL7USER) -c -m 0644 $(RESPS) $(DESTDIR)/responders
	install -o $(HHL7USER) -g $(HHL7USER) -c -m 0644 $(TEMPS) $(DESTDIR)/templates
	install -o $(HHL7USER) -g $(HHL7USER) -c -m 0644 $(DATAS) $(DESTDIR)/datafiles
	install -c -m 0644 $(MAN) $(MANDIR)
