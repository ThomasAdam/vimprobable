GCC    = gcc
LIBS   = gtk+-2.0
LIBS  += webkit-1.0
LIBS  += libsoup-2.4
FLAGS  = `pkg-config --cflags --libs $(LIBS)`
SOURCE = main.c
TARGET = vimpression
INSTALLDIR = /usr/local/bin

all: $(TARGET)

$(TARGET): main.c config.h
	$(GCC) $(FLAGS) -Wall -o $(TARGET) $(SOURCE)

clean:
	rm -f $(TARGET)

install: all uninstall
	cp $(TARGET) $(INSTALLDIR)

uninstall:
	rm -f $(INSTALLDIR)/$(TARGET)
