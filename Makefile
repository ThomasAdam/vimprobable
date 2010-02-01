GCC    = gcc
LIBS   = gtk+-2.0
LIBS  += webkit-1.0
LIBS  += libsoup-2.4
FLAGS  = `pkg-config --cflags --libs $(LIBS)` -Wall -Wstrict-prototypes -ansi
SOURCE = main.c
TARGET = vimprobable2
PREFIX = /usr/local

all: $(TARGET)

hintingmode.h: input_hinting_mode.js
	perl ./js-merge-helper.pl

callbacks.o:	callbacks.c
	gcc -c $(FLAGS) -o callbacks.o callbacks.c

utilities.o:	utilities.c
	gcc -c $(FLAGS) -o utilities.o utilities.c

main.o:	main.c config.h hintingmode.h
	gcc -c $(FLAGS) -o main.o main.c

$(TARGET): main.o utilities.o callbacks.o
	$(GCC) $(FLAGS) -Wall -o $(TARGET) main.o utilities.o callbacks.o

clean:
	rm -f $(TARGET); rm -f *.o; rm -f hintingmode.h

install: all uninstall
	strip $(TARGET) && cp $(TARGET) $(PREFIX)/bin && chmod 755 $(PREFIX)/bin/$(TARGET)
	cp vimprobable2.1 $(PREFIX)/man/man1 && chmod 644 $(PREFIX)/man/man1/vimprobable2.1
	cp vimprobablerc.1 $(PREFIX)/man/man1 && chmod 644 $(PREFIX)/man/man1/vimprobablerc.1

uninstall:
	rm -f $(PREFIX)/bin/$(TARGET)
	rm -f $(PREFIX)/man/man1/vimprobable2.1
	rm -f $(PREFIX)/man/man1/vimprobablerc.1
