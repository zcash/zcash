AC_DEFUN([BITCOIN_FIND_BDB62],[
  AC_MSG_CHECKING([for Berkeley DB C++ headers])
  BDB_CPPFLAGS=
  BDB_LIBS=
  bdbpath=X
  bdb62path=X
  bdbdirlist=
  for _vn in 6.2 62 6 ''; do
    for _pfx in b lib ''; do
      bdbdirlist="$bdbdirlist ${_pfx}db${_vn}"
    done
  done
  for searchpath in $bdbdirlist ''; do
    test -n "${searchpath}" && searchpath="${searchpath}/"
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
      #include <${searchpath}db_cxx.h>
    ]],[[
      #if !((DB_VERSION_MAJOR == 6 && DB_VERSION_MINOR >= 2) || DB_VERSION_MAJOR > 6)
        #error "failed to find bdb 6.2+"
      #endif
    ]])],[
      if test "x$bdbpath" = "xX"; then
        bdbpath="${searchpath}"
      fi
    ],[
      continue
    ])
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
      #include <${searchpath}db_cxx.h>
    ]],[[
      #if !(DB_VERSION_MAJOR == 6 && DB_VERSION_MINOR == 2)
        #error "failed to find bdb 6.2"
      #endif
    ]])],[
      bdb62path="${searchpath}"
      break
    ],[])
  done
  if test "x$bdbpath" = "xX"; then
    AC_MSG_RESULT([no])
    AC_MSG_ERROR([libdb_cxx headers missing, Bitcoin Core requires this library for wallet functionality (--disable-wallet to disable wallet functionality)])
  elif test "x$bdb62path" = "xX"; then
    BITCOIN_SUBDIR_TO_INCLUDE(BDB_CPPFLAGS,[${bdbpath}],db_cxx)
    AC_ARG_WITH([incompatible-bdb],[AS_HELP_STRING([--with-incompatible-bdb], [allow using a bdb version other than 6.2])],[
      AC_MSG_WARN([Found Berkeley DB other than 6.2; wallets opened by this build will not be portable!])
    ],[
      AC_MSG_ERROR([Found Berkeley DB other than 6.2, required for portable wallets (--with-incompatible-bdb to ignore or --disable-wallet to disable wallet functionality)])
    ])
  else
    BITCOIN_SUBDIR_TO_INCLUDE(BDB_CPPFLAGS,[${bdb62path}],db_cxx)
    bdbpath="${bdb62path}"
  fi
  AC_SUBST(BDB_CPPFLAGS)
  
  # TODO: Ideally this could find the library version and make sure it matches the headers being used
  for searchlib in db_cxx-6.2 db_cxx; do
    AC_CHECK_LIB([$searchlib],[main],[
      BDB_LIBS="-l${searchlib}"
      break
    ])
  done
  if test "x$BDB_LIBS" = "x"; then
      AC_MSG_ERROR([libdb_cxx missing, Bitcoin Core requires this library for wallet functionality (--disable-wallet to disable wallet functionality)])
  fi
  AC_SUBST(BDB_LIBS)
])
