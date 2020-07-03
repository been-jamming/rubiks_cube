ifeq ($(PREFIX),)
	PREFIX := /usr
endif

default: 3d.c CAM-curses-ascii-matcher/CAM.c menu.c
	cc 3d.c CAM-curses-ascii-matcher/CAM.c menu.c -Wall -O3 -ffast-math -lm -lncurses -o rubiks_cube

install: rubiks_cube
	install -d $(DESTDIR)$(PREFIX)/bin
	install -C rubiks_cube $(DESTDIR)$(PREFIX)/bin/rubiks_cube
	install -d $(DESTDIR)$(PREFIX)/share/licenses/rubiks_cube
	install -C LICENSE $(DESTDIR)$(PREFIX)/share/licenses/rubiks_cube/LICENSE
