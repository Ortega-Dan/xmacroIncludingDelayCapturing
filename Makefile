#Q				= @
VERSION			= 0.4.6
CFLAGS			= -O2 -g -D_GNU_SOURCE
PROGRAMS		= xmacrorec2 xmacroplay 
CONFIG_C		= config1.c
all: $(PROGRAMS) 

%: %.c $(CONFIG_C) chartbl.h Makefile
	$(info ==> Compinging: $@)	
	$(Q)$(CC) $(CFLAGS)  -I/usr/X11R6/include -Wall -DVERSION=\"$(VERSION)\"  -DPROG="\"$@\"" $<  $(CONFIG_C) -o $@ -L/usr/X11R6/lib -lX11 -lXtst 

clean:
	$(Q)rm $(PROGRAMS)

deb:
	umask 022 && epm -f deb -nsm xmacro

rpm:
	umask 022 && epm -f rpm -nsm xmacro
dist:
	mkdir xmacro-$(VERSION)
	cp *.list *.c *.h Makefile README COPYING xmacro-$(VERSION)/
	cp -r doc/ xmacro-$(VERSION)/
	tar  -cvvf xmacro-$(VERSION).tar xmacro-$(VERSION)

	gzip xmacro-$(VERSION).tar
	rm -rf xmacro-$(VERSION)
