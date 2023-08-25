CC     = gcc
CFLAGS = -O2 -Wall -fcommon
INCDIR = -I/usr/include/json-c -I/usr/include/microhttpd
DESTDIR= /usr
MANDIR = /usr/share/man
LIBDIR = 
LIBS   = -ljson-c -lmicrohttpd
OBJS   = hhl7webpages.o hhl7web.o hhl7net.o hhl7utils.o hhl7json.o hhl7.o
BIN    = hhl7
MAN    = hhl7.1

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
	install -c -s -m 0755 $(BIN) $(DESTDIR)/bin
	install -c -m 0644 $(MAN) $(MANDIR)/man1 
