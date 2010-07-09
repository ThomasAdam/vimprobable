TARGET = vimprobable2

# Objectfiles, needed for $(TARGET)
OBJ = main.o utilities.o callbacks.o
# Manpages
MAN = vimprobable2.1 vimprobablerc.1
# Used libraries to get needed CFLAGS and LDFLAGS form pkg-config
LIBS = gtk+-2.0 webkit-1.0 libsoup-2.4
# Files to removo by clean target
CLEAN = $(TARGET) $(OBJ) $(DEPS) hintingmode.h
# Files to install by install target or remove by uninstall target
INSTALL = $(BINDIR)/$(TARGET) $(addprefix $(MANDIR)/man1/,$(MAN))

CFLAGS += `pkg-config --cflags $(LIBS)`
LDFLAGS += `pkg-config --libs $(LIBS)` -g -ggdb

PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
MANDIR ?= $(PREFIX)/man
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

main.o: hintingmode.h
hintingmode.h: input_hinting_mode.js
	perl ./js-merge-helper.pl

$(TARGET): $(OBJ)
	$(CC) $^ $(LDFLAGS) -o $@

.PHONY: clean install uninstall
clean:
	-rm -f $(CLEAN)
install: $(addprefix $(DESTDIR)/,$(INSTALL))
uninstall:
	rm -f $(INSTALL)

# pattern rule to inslall executabels
$(DESTDIR)/$(BINDIR)/%: ./%
	-[ -e '$(@D)' ] || mkdir -p '$(@D)' && chmod $(DMOD) '$(@D)'
	cp -f '$<' '$@'
	chmod $(EXECMOD) '$@'

# pattern rule to install manpages
$(DESTDIR)/$(MANDIR)/man1/%: ./%
	-[ -e '$(@D)' ] || mkdir -p '$(@D)' && chmod $(DMOD) '$(@D)'
	cp -f '$<' '$@'
	chmod $(FMOD) '$@'

%.o: %.c
	$(CC) -MMD -c $(CFLAGS) $< -o $@ 
