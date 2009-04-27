GCC    = gcc
LIBS   = gtk+-2.0
LIBS  += webkit-1.0
FLAGS  = `pkg-config --cflags --libs $(LIBS)`
SOURCE = main.c
TARGET = webkitbrowser

all:
	$(GCC) $(FLAGS) -o $(TARGET) $(SOURCE)

clean:
	rm -f $(TARGET)
