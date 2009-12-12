GCC    = gcc
LIBS   = gtk+-2.0
LIBS  += webkit-1.0
LIBS  += libsoup-2.4
FLAGS  = `pkg-config --cflags --libs $(LIBS)`
SOURCE = main.c
TARGET = vimprobable
INSTALLDIR = /usr/local/bin

all: $(TARGET)

hintingmode.h: input_hinting_mode.js
	perl ./js-merge-helper.pl

$(TARGET): main.c config.h hintingmode.h
	$(GCC) $(FLAGS) -Wall -o $(TARGET) $(SOURCE)

clean:
	rm -f $(TARGET)

install: all uninstall
	cp $(TARGET) $(INSTALLDIR) && chmod 755 $(INSTALLDIR)/$(TARGET)

uninstall:
	rm -f $(INSTALLDIR)/$(TARGET)
