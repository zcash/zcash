#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.56)
AC_INIT([Rogue],[5.4.4], [yendor@rogueforge.net])
AC_CONFIG_SRCDIR([armor.c])
AC_CONFIG_HEADER([config.h])
AC_CONFIG_FILES([Makefile rogue.6 rogue.cat rogue.doc rogue.html rogue.me])
AC_CANONICAL_SYSTEM([])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([arpa/inet.h sys/utsname.h pwd.h fcntl.h limits.h nlist.h stdlib.h string.h sys/ioctl.h termios.h unistd.h utmp.h term.h ncurses/term.h process.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_CONST
AC_TYPE_UID_T
AC_TYPE_SIZE_T
AC_STRUCT_TM
MP_WITH_CURSES
# Checks for library functions.
AC_FUNC_FORK
AC_PROG_GCC_TRADITIONAL
AC_FUNC_LSTAT
AC_FUNC_LSTAT_FOLLOWS_SLASHED_SYMLINK
AC_TYPE_SIGNAL
AC_FUNC_STAT
AC_FUNC_VPRINTF
AC_CHECK_FUNCS([erasechar killchar alarm getpass memset setenv strchr nlist _spawnl spawnl getpwuid loadav getloadavg strerror setresgid setregid setgid setresuid setreuid setuid getuid getgid])

AC_CHECK_PROG([NROFF], [nroff], [nroff],)
AC_CHECK_PROG([GROFF], [groff], [groff],)
AC_CHECK_PROG([COLCRT], [colcrt], [colcrt],)
AC_CHECK_PROG([TBL], [tbl], [tbl],)
AC_CHECK_PROG([SED], [sed], [sed],)

AC_ARG_WITH(program-name, AC_HELP_STRING([--with-program-name=NAME],[alternate executable name]),[progname="$withval" ], [progname="rogue"] )
PROGRAM=$progname
AC_SUBST(PROGRAM)

AC_ARG_ENABLE(setgid, AC_HELP_STRING([--enable-setgid=NAME],[install executable as setgid with group ownership of NAME @<:@default=no@:>@])],[],[])
AC_MSG_CHECKING([if using setgid execute bit])
if test "x$enable_setgid" = "xno" ; then
GROUPOWNER=
elif test "x$enable_setgid" = "xyes" ; then
GROUPOWNER=games
elif test "x$enable_setgid" = "x" ; then
GROUPOWNER=
else
GROUPOWNER=$enable_setgid
fi

if test "x$GROUPOWNER" != "x" ; then
AC_DEFINE_UNQUOTED([GROUPOWNER],[$GROUPOWNER], [Define to group owner of setgid executable])
AC_MSG_RESULT([$GROUPOWNER])
else
AC_MSG_RESULT([no])
fi

AC_SUBST(GROUPOWNER)

AC_ARG_ENABLE([scorefile],[AC_HELP_STRING([--enable-scorefile=SCOREFILE], [enable scoreboard with given filename])],[],[])
AC_MSG_CHECKING([for scoreboard file])
if test "x$enable_scorefile" = "xno" ; then
SCOREFILE=
elif test "x$enable_scorefile" = "xyes" ;  then
SCOREFILE=$progname.scr
elif test "x$enable_scorefile" = "x" ; then
SCOREFILE=$progname.scr
else
SCOREFILE=$enable_scorefile
fi

if test "x$SCOREFILE" != "x" ; then
AC_DEFINE_UNQUOTED([SCOREFILE], ["$SCOREFILE"], [Define to file to use for scoreboard])
AC_MSG_RESULT([$SCOREFILE])
else
AC_MSG_RESULT([disabled])
fi

AC_SUBST(SCOREFILE)

AC_ARG_ENABLE([lockfile],[AC_HELP_STRING([--enable-lockfile=LOCKFILE], [enable scoreboard lockfile with given filename])],[],[])
AC_MSG_CHECKING([for scoreboard lockfile file])
if test "x$enable_lockfile" = "xno" ; then
LOCKFILE=
elif test "x$enable_lockfile" = "xyes" ; then
LOCKFILE=$progname.lck
elif test "x$enable_lockfile" = "x" ; then
LOCKFILE=$progname.lck
else
LOCKFILE=$enable_lockfile
fi

if test "x$LOCKFILE" != "x" ; then
AC_DEFINE_UNQUOTED([LOCKFILE], ["$LOCKFILE"], [Define to file to use for scoreboard lockfile])
AC_MSG_RESULT([$LOCKFILE])
else
AC_MSG_RESULT([disabled])
fi

AC_SUBST(LOCKFILE)

AC_ARG_ENABLE([wizardmode],[AC_HELP_STRING([--enable-wizardmode], [enable availability of wizard mode @<:@default=no@:>@])],[],[])
AC_MSG_CHECKING([if wizard mode is enabled])
if test "x$enable_wizardmode" = "xno" ; then
AC_MSG_RESULT([no])
elif test "x$enable_wizardmode" = "x" ; then
AC_MSG_RESULT([no])
else
AC_DEFINE([MASTER], [], [Define to include wizard mode])
if test "x$enable_wizardmode" != "xyes" ; then
AC_DEFINE_UNQUOTED([PASSWD],[$enable_wizardmode], [Define crypt(3) wizard mode password])
fi
AC_MSG_RESULT([yes])
fi

AC_ARG_ENABLE([allscores],[AC_HELP_STRING([--enable-allscores], [enable scoreboard to show top scores, not just top players @<:@default=yes@:>@])],[],[enable_allscores=yes])
AC_MSG_CHECKING([if allscores is enabled])
if test "x$enable_allscores" = "xyes" ; then
AC_DEFINE([ALLSCORES], [1], [Define if scorefile is top scores, not top players])
AC_MSG_RESULT([yes])
else
AC_MSG_RESULT([no])
fi

AC_ARG_ENABLE([checktime],[AC_HELP_STRING([--enable-checktime], [enable checktime @<:@default=no@:>@])],[],[])
AC_MSG_CHECKING([if checktime is enabled])
if test "x$enable_checktime" = "xyes" ; then
AC_DEFINE([CHECKTIME], [1], [Define if checktime feature should be enabled])
AC_MSG_RESULT([yes])
else
AC_MSG_RESULT([no])
fi

AC_ARG_ENABLE([maxload],[AC_HELP_STRING([--enable-maxload], [enable maxload @<:@default=no@:>@])],[],[])
AC_MSG_CHECKING([runtime execution limit (maximum system load average)])
if test "x$enable_maxload" = "xyes" ; then
AC_DEFINE([MAXLOAD], [100], [Define if maxload feature should be enabled])
AC_MSG_RESULT([100])
elif test "x$enable_maxload" = "x" ; then
AC_MSG_RESULT([unlimited])
elif test "x$enable_maxload" = "xno" ; then
AC_MSG_RESULT([unlimited])
else
AC_DEFINE_UNQUOTED([MAXLOAD], [$enable_maxload], [Define if maxload feature should be enabled])
AC_MSG_RESULT([$enable_maxload])
fi

AC_ARG_ENABLE([maxusers],[AC_HELP_STRING([--enable-maxusers], [enable maxuser @<:@default=no@:>@])],[],[])
AC_MSG_CHECKING([runtime execution limit (maximum online system users)])
if test "x$enable_maxusers" = "xyes" ; then
AC_DEFINE([MAXUSERS], [100], [Define if maxusers feature should be enabled])
AC_MSG_RESULT([100])
elif test "x$enable_maxusers" = "x" ; then
AC_MSG_RESULT([unlimited])
elif test "x$enable_maxload" = "xno" ; then 
AC_MSG_RESULT([unlimited])
else
AC_DEFINE_UNQUOTED([MAXLOAD], [$enable_maxusers], [Define if maxusers feature should be enabled])
AC_MSG_RESULT([$enable_maxusers])
fi

AC_ARG_ENABLE([numscores],[AC_HELP_STRING([--enable-numscores], [number of scores to store in scoreboard @<:@default=10@:>@])],[],[])
AC_MSG_CHECKING([what the number of scores to store in scoreboard is])
if test "x$numscores" = "xyes" ; then
AC_DEFINE([NUMSCORES], [10], [number of scores to store in scoreboard])
AC_MSG_RESULT([10])
elif test "x$enable_numscores" = "x" ; then
AC_DEFINE([NUMSCORES], [10], [number of scores to store in scoreboard])
AC_MSG_RESULT([10])
elif test "x$enable_numscores" = "xno" ; then 
AC_DEFINE([NUMSCORES], [10], [number of scores to store in scoreboard])
AC_MSG_RESULT([10])
else
AC_DEFINE_UNQUOTED([NUMSCORES], [$enable_numscores], [number of scores to store in scoreboard])
AC_MSG_RESULT([$enable_numscores])
fi

AC_ARG_ENABLE([numname],[AC_HELP_STRING([--enable-numname], [word for number of scores to store in scoreboard @<:@default=Ten@:>@])],[],[])
AC_MSG_CHECKING([word for the number of scores to store in scoreboard is])
if test "x$enable_numname" = "xyes" ; then
AC_DEFINE([NUMNAME], ["Ten"], [word for the number of scores to store in scoreboard])
AC_MSG_RESULT([Ten])
elif test "x$enable_numname" = "x" ; then
AC_DEFINE([NUMNAME], ["Ten"], [word for the number of scores to store in scoreboard])
AC_MSG_RESULT([Ten])
elif test "x$enable_numname" = "xno" ; then 
AC_DEFINE([NUMNAME], ["Ten"], [word for the number of scores to store in scoreboard])
AC_MSG_RESULT([Ten])
else
AC_DEFINE_UNQUOTED([NUMNAME], ["$enable_numname"], [word for the number of scores to store in scoreboard])
AC_MSG_RESULT([$enable_numname])
fi

AC_ARG_ENABLE([loadav],[AC_HELP_STRING([--enable-loadav=NAMELIST], [use program's load average function (unlikely to work) @<:@default=no@:>@])],[],[])
AC_MSG_CHECKING([whether to use program's built in load average function])
if test "x$enable_loadav" = "xyes" ; then
AC_DEFINE([LOADAV], [], [define if we should use program's load average function instead of system])
AC_DEFINE([NAMELIST], [/vmunix], [kernel file to pass to nlist() when reading load average (unlikely to work)])
AC_MSG_RESULT([/vmunix])
elif test "x$enable_loadav" = "x" ; then
AC_MSG_RESULT([no])
elif test "x$enable_loadav" = "xno" ; then 
AC_MSG_RESULT([no])
else
AC_DEFINE([LOADAV], [], [define if we should use program's load average function instead of system])
AC_DEFINE_UNQUOTED([NAMELIST], [$enable_loadav], [kernel file to pass to nlist() when reading load average (unlikely to work)])
AC_MSG_RESULT([$enable_loadav])
fi

AC_ARG_ENABLE([ucount],[AC_HELP_STRING([--enable-ucount=UTMPFILE], [use program's own function to count users (unlikely to work) @<:@default=no@:>@])],[],[])
AC_MSG_CHECKING([whether to use program's built in user counting function])
if test "x$enable_ucount" = "xyes" ; then
AC_DEFINE([UCOUNT], [], [define if we should use program's user counting function instead of system's])
AC_DEFINE([UTMP], [/etc/utmp], [utmp like file to pass to ucount() when counting online users (unlikely to work)])
AC_MSG_RESULT([/etc/utmp])
elif test "x$enable_ucount" = "x" ; then
AC_MSG_RESULT([no])
elif test "x$enable_count" = "xno" ; then 
AC_MSG_RESULT([no])
else
AC_DEFINE([UCOUNT], [], [define if we should use program's user counting function instead of system's])
AC_DEFINE_UNQUOTED([UTMP], [$enable_ucount], [utmp like file to pass to ucount() when counting online users (unlikely to work)])
AC_MSG_RESULT([$enable_ucount])
fi

TARGET=$target
AC_SUBST(TARGET)

AC_MSG_CHECKING([whether to docdir is defined])
if test "x$docdir" = "x" ; then
AC_MSG_RESULT([docdir undefined])
docdir=\${datadir}/doc/\${PACKAGE_TARNAME}
AC_SUBST(docdir)
else
AC_MSG_RESULT([docdir defined])
fi

AC_OUTPUT
