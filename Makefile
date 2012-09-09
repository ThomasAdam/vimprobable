TARGET = vimprobable2

# Objectfiles, needed for $(TARGET)
#OBJ = main.o utilities.o callbacks.o
OBJ = $(patsubst %.c, %.o, $(wildcard *.c))
# Manpages
MAN1 = vimprobable2.1
MAN5 = vimprobablerc.5
# Used libraries to get needed CFLAGS and LDFLAGS form pkg-config
LIBS = gtk+-2.0 webkit-1.0 libsoup-2.4
# Files to removo by clean target
CLEAN = $(TARGET) $(OBJ) $(DEPS) javascript.h
# Files to install by install target or remove by uninstall target
MANINSTALL = $(addprefix $(MANDIR)/man1/,$(MAN1)) \
             $(addprefix $(MANDIR)/man5/,$(MAN5))
INSTALL = $(BINDIR)/$(TARGET) $(MANINSTALL)

# DEBUG build?  Off by default
V_DEBUG = 0

CFLAGS += `pkg-config --cflags $(LIBS)` -D_XOPEN_SOURCE=500
LDFLAGS += `pkg-config --libs $(LIBS)` -lX11 -lXext

# TA:  This is a pretty stringent list of warnings to bail on!
ifeq ($(V_DEBUG),1)
CFLAGS += -g -ggdb -ansi -Wstrict-prototypes
CFLAGS += -Wno-long-long -Wall -Wmissing-declarations
endif

PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
MANDIR ?= $(PREFIX)/share/man
# Mode bits for normal not executable files
FMOD ?= 0644
# Mode bits for directories
DMOD ?= 0755
# Mode bits for executables
EXECMOD ?= 0755
# Destination directory to install files
DESTDIR ?= /

# auto garerated dependancies for object files
DEPS = $(OBJ:%.o=%.d)

all: $(TARGET)

-include $(DEPS)

main.o: javascript.h
javascript.h: hinting.js
	perl ./js-merge-helper.pl

$(TARGET): $(OBJ)
	$(CC) $^ $(LDFLAGS) -o $@

.PHONY: clean install uninstall
clean:
	-rm -f $(CLEAN)
install: $(addprefix $(DESTDIR)/,$(INSTALL))
uninstall:
	rm -f $(addprefix $(DESTDIR)/,$(INSTALL))

# pattern rule to inslall executabels
$(DESTDIR)/$(BINDIR)/%: ./%
	-[ -e '$(@D)' ] || mkdir -p '$(@D)' && chmod $(DMOD) '$(@D)'
	cp -f '$<' '$@'
	-strip -s '$@'
	chmod $(EXECMOD) '$@'

# pattern rules to install manpages
$(DESTDIR)/$(MANDIR)/man1/%: ./%
	-[ -e '$(@D)' ] || mkdir -p '$(@D)' && chmod $(DMOD) '$(@D)'
	cp -f '$<' '$@'
	chmod $(FMOD) '$@'

$(DESTDIR)/$(MANDIR)/man5/%: ./%
	-[ -e '$(@D)' ] || mkdir -p '$(@D)' && chmod $(DMOD) '$(@D)'
	cp -f '$<' '$@'
	chmod $(FMOD) '$@'

%.o: %.c
	$(CC) -MMD -c $(CFLAGS) $< -o $@ 
