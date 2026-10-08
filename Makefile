CC      = gcc
CFLAGS  = -O2 -Wall -Wextra -pedantic -std=c11
LDFLAGS =
TARGET  = highlight

# Enable compiler and linker hardening by default.
ifeq ($(shell uname -s),Linux)
HARDENING_CFLAGS ?= -D_FORTIFY_SOURCE=3 -fstack-protector-strong -fstack-clash-protection -Wformat=2 -Wformat-security -Werror=format-security -fPIE
HARDENING_LDFLAGS ?= -pie -Wl,-z,relro,-z,now,-z,noexecstack

ifneq ($(filter x86_64 i386 i686,$(shell uname -m)),)
HARDENING_CFLAGS += -fcf-protection=full
endif
else
HARDENING_CFLAGS ?= -D_FORTIFY_SOURCE=2 -fstack-protector-strong -Wformat=2 -Wformat-security -Werror=format-security -fPIE
HARDENING_LDFLAGS ?= -pie
endif

SRC = highlight.c
OBJ = $(SRC:.c=.o)

.PHONY: all clean install uninstall

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS) $(HARDENING_LDFLAGS)

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(HARDENING_CFLAGS) -c $< -o $@

install: $(TARGET)
	install -Dm755 $(TARGET) /usr/local/bin/$(TARGET)

uninstall:
	rm -f /usr/local/bin/$(TARGET)

clean:
	rm -f $(TARGET) $(OBJ)
