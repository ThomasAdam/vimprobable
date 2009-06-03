GCC    = gcc
LIBS   = gtk+-2.0
LIBS  += webkit-1.0
LIBS  += libsoup-2.4
FLAGS  = `pkg-config --cflags --libs $(LIBS)`
SOURCE = main.c
SOURCE_JS = tmp.c
TARGET = vimpression
INSTALLDIR = /usr/local/bin

all: $(TARGET)

$(TARGET): main.c config.h
	perl ./js-merge-helper.pl
	$(GCC) $(FLAGS) -Wall -o $(TARGET) $(SOURCE_JS)
	rm $(SOURCE_JS)

clean:
	rm -f $(TARGET)

install: all uninstall
	cp $(TARGET) $(INSTALLDIR)

uninstall:
	rm -f $(INSTALLDIR)/$(TARGET)
