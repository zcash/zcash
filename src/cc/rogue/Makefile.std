#
# Makefile for rogue
# @(#)Makefile	4.21 (Berkeley) 02/04/99
#
# Rogue: Exploring the Dungeons of Doom
# Copyright (C) 1980-1983, 1985, 1999 Michael Toy, Ken Arnold and Glenn Wichman
# All rights reserved.
#
# See the file LICENSE.TXT for full copyright and licensing information.
#

DISTNAME = rogue5.4.4
PROGRAM  = rogue54
O        = o
HDRS     = rogue.h extern.h score.h
OBJS1    = vers.$(O) extern.$(O) armor.$(O) chase.$(O) command.$(O) \
           daemon.$(O) daemons.$(O) fight.$(O) init.$(O) io.$(O) list.$(O) \
	   mach_dep.$(O) main.$(O) mdport.$(O) misc.$(O) monsters.$(O) \
	   move.$(O) new_level.$(O)
OBJS2    = options.$(O) pack.$(O) passages.$(O) potions.$(O) rings.$(O) \
           rip.$(O) rooms.$(O) save.$(O) scrolls.$(O) state.$(O) sticks.$(O) \
	   things.$(O) weapons.$(O) wizard.$(O) xcrypt.$(O)
OBJS     = $(OBJS1) $(OBJS2)
CFILES   = vers.c extern.c armor.c chase.c command.c daemon.c \
	   daemons.c fight.c init.c io.c list.c mach_dep.c \
	   main.c  mdport.c misc.c monsters.c move.c new_level.c \
	   options.c pack.c passages.c potions.c rings.c rip.c \
	   rooms.c save.c scrolls.c state.c sticks.c things.c \
	   weapons.c wizard.c xcrypt.c
MISC_C   = findpw.c scedit.c scmisc.c
DOCSRC   = rogue.me.in rogue.6.in rogue.doc.in rogue.html.in rogue.cat.in
DOCS     = $(PROGRAM).doc $(PROGRAM).html $(PROGRAM).cat $(PROGRAM).me \
           $(PROGRAM).6
AFILES   = configure Makefile.in configure.ac config.h.in config.sub config.guess \
           install-sh rogue.6.in rogue.me.in rogue.html.in rogue.doc.in rogue.cat.in
MISC     = Makefile.std LICENSE.TXT rogue54.sln rogue54.vcproj rogue.spec \
           rogue.png rogue.desktop
CC       = gcc
FEATURES = -DALLSCORES -DSCOREFILE=\"$(SCOREFILE)\" -DLOCKFILE=\"$(LOCKFILE)\"
CPPFLAGS =
CFLAGS   = -O3
LDFLAGS  =
LIBS     = -lcurses
RM       = rm -f
MAKEFILE = -f Makefile.std
SCOREFILE= $(PROGRAM).scr
LOCKFILE = $(PROGRAM).lck
OUTFLAG  = -o
EXE      =

.SUFFIXES: .obj

.c.obj:
	$(CC) $(CFLAGS) $(CPPFLAGS) $(FEATURES) /c $*.c
    
.c.o:
	$(CC) $(CFLAGS) $(CPPFLAGS) $(FEATURES) -c $*.c
    
$(PROGRAM): $(HDRS) $(OBJS) fixdocs
	$(CC) $(LDFLAGS) $(OBJS) $(LIBS) $(OUTFLAG)$@$(EXE)
 
clean:
	$(RM) $(OBJS1)
	$(RM) $(OBJS2)
	$(RM) core a.exe a.out a.exe.stackdump $(PROGRAM) $(PROGRAM).exe $(PROGRAM).lck
	$(RM) $(PROGRAM).tar $(PROGRAM).tar.gz $(PROGRAM).zip 
	$(RM) $(DISTNAME)/*
    
dist.src:
	$(MAKE) $(MAKEFILE) clean
	mkdir $(DISTNAME)
	cp $(CFILES) $(HDRS) $(MISC) $(AFILES) $(DISTNAME)
	tar cf $(DISTNAME)-src.tar $(DISTNAME)
	gzip -f $(DISTNAME)-src.tar
	rm -fr $(DISTNAME)

findpw: findpw.c xcrypt.o mdport.o xcrypt.o
	$(CC) -s -o findpw findpw.c xcrypt.o mdport.o -lcurses

scedit: scedit.o scmisc.o vers.o mdport.o xcrypt.o
	$(CC) -s -o scedit vers.o scedit.o scmisc.o mdport.o xcrypt.o -lcurses

scmisc.o scedit.o:
	$(CC) -O -c $(SF) $*.c

doc.nroff:
	tbl rogue.me | nroff -me | colcrt - > rogue.doc
	nroff -man rogue.6 | colcrt - > rogue.cat

doc.groff:
	groff -P-c -t -me -Tascii rogue.me | sed -e 's/.\x08//g' > rogue.doc
	groff -man rogue.6 | sed -e 's/.\x08//g' > rogue.cat

fixdocs:
	sed -e 's/@PROGRAM@/$(PROGRAM)/' -e 's/@SCOREFILE@/$(SCOREFILE)/' rogue.6.in > $(PROGRAM).6
	sed -e 's/@PROGRAM@/$(PROGRAM)/' -e 's/@SCOREFILE@/$(SCOREFILE)/' rogue.me.in > $(PROGRAM).me
	sed -e 's/@PROGRAM@/$(PROGRAM)/' -e 's/@SCOREFILE@/$(SCOREFILE)/' rogue.html.in > $(PROGRAM).html
	sed -e 's/@PROGRAM@/$(PROGRAM)/' -e 's/@SCOREFILE@/$(SCOREFILE)/' rogue.doc.in > $(PROGRAM).doc
	sed -e 's/@PROGRAM@/$(PROGRAM)/' -e 's/@SCOREFILE@/$(SCOREFILE)/' rogue.cat.in > $(PROGRAM).cat

dist.irix:
	$(MAKE) $(MAKEFILE) clean
	$(MAKE) $(MAKEFILE)  CC=cc $(PROGRAM)
	tar cf $(DISTNAME)-irix.tar $(PROGRAM) LICENSE.TXT $(DOCS)
	gzip -f $(DISTNAME)-irix.tar

dist.aix:
	$(MAKE) $(MAKEFILE) clean
	$(MAKE) $(MAKEFILE) CC=xlc CFLAGS="-qmaxmem=16768 -O3 -qstrict" $(PROGRAM)
	tar cf $(DISTNAME)-aix.tar $(PROGRAM) LICENSE.TXT $(DOCS)
	gzip -f $(DISTNAME)-aix.tar

dist.linux:
	$(MAKE) $(MAKEFILE) clean
	$(MAKE) $(MAKEFILE) $(PROGRAM)
	tar cf $(DISTNAME)-linux.tar $(PROGRAM) LICENSE.TXT $(DOCS)
	gzip -f $(DISTNAME)-linux.tar
	
dist.interix:
	@$(MAKE) $(MAKEFILE) clean
	@$(MAKE) $(MAKEFILE) CFLAGS="-ansi" $(PROGRAM)
	tar cf $(DISTNAME)-interix.tar $(PROGRAM) LICENSE.TXT $(DOCS)
	gzip -f $(DISTNAME)-interix.tar
	
dist.cygwin:
	@$(MAKE) $(MAKEFILE) --no-print-directory clean
	@$(MAKE) $(MAKEFILE) CPPFLAGS="-I/usr/include/ncurses" --no-print-directory $(PROGRAM)
	tar cf $(DISTNAME)-cygwin.tar $(PROGRAM).exe LICENSE.TXT $(DOCS)
	gzip -f $(DISTNAME)-cygwin.tar

#
# Use MINGW32-MAKE to build this target
#
dist.mingw32:
	@$(MAKE) $(MAKEFILE) --no-print-directory RM="cmd /c del" clean
	@$(MAKE) $(MAKEFILE) --no-print-directory CPPFLAGS="-I../pdcurses" LIBS="../pdcurses/pdcurses.a" $(PROGRAM)
	cmd /c del $(DISTNAME)-mingw32.zip
	zip $(DISTNAME)-mingw32.zip $(PROGRAM).exe LICENSE.TXT $(DOCS)
	
dist.djgpp:
	@$(MAKE) $(MAKEFILE) --no-print-directory clean
	@$(MAKE) $(MAKEFILE) --no-print-directory LDFLAGS="-L$(DJDIR)/LIB" \
	LIBS="-lpdcurses" $(PROGRAM)
	rm -f $(DISTNAME)-djgpp.zip
	zip $(DISTNAME)-djgpp.zip $(PROGRAM) LICENSE.TXT $(DOCS)

#
# Use NMAKE to build this targer
#

dist.win32:
	@$(MAKE) $(MAKEFILE) /NOLOGO O="obj" RM="-del" clean
	@$(MAKE) $(MAKEFILE) /NOLOGO O="obj" CC="CL" \
	    LIBS="..\pdcurses\pdcurses.lib shell32.lib user32.lib Advapi32.lib" \
	    EXE=".exe" OUTFLAG="/Fe" CPPFLAGS="-I..\pdcurses" \
	    CFLAGS="-nologo -Ox -wd4033 -wd4716" $(PROGRAM)
	-del $(DISTNAME)-win32.zip
	zip $(DISTNAME)-win32.zip $(PROGRAM).exe LICENSE.TXT $(DOCS)
