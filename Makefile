CFLAGS=-Wall -Wextra -Werror -Iinclude
LDFLAGS=-lwayland-egl -lwayland-client -lEGL -lGL

WAYLAND_PROTOCOL_DIR=$(shell pkg-config wayland-protocols --variable=pkgdatadir)

HEADERS=xdg-shell-client-protocol.h
SOURCES=src/xdg-shell-client-protocol.c src/main.c

all: wayland-egl

wayland-egl: xdg-shell-client-protocol.h xdg-shell-client-protocol.c
	$(CC) $(CFLAGS) -o $@ $(SOURCES) $(LDFLAGS)

xdg-shell-client-protocol.h:
	wayland-scanner client-header /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml include/xdg-shell-client-protocol.h

xdg-shell-client-protocol.c:
	wayland-scanner private-code /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml src/xdg-shell-client-protocol.c

clean:
	$(RM) wayland-egl include/xdg-shell-client-protocol.h src/xdg-shell-client-protocol.c
