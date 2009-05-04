GCC    = gcc
LIBS   = gtk+-2.0
LIBS  += webkit-1.0
FLAGS  = `pkg-config --cflags --libs $(LIBS)`
SOURCE = main.c
TARGET = webkitbrowser
INSTALLDIR = /usr/local/bin

all:
	$(GCC) $(FLAGS) -Wall -o $(TARGET) $(SOURCE)

clean:
	rm -f $(TARGET)

install:
	cp $(TARGET) $(INSTALLDIR)

uninstall:
	rm -f $(INSTALLDIR)/$(TARGET)
