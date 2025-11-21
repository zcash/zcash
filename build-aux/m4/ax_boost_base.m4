# ===========================================================================
# AX_BOOST_BASE: Optimized and Cleaned Version
# ===========================================================================

# serial 51

# Program to check if the required Boost version is available.
# Uses a static assertion (sizeof(char[...])) based on BOOST_VERSION macro.
m4_define([_AX_BOOST_BASE_PROGRAM],
    [AC_LANG_PROGRAM([[
#include <boost/version.hpp>
]],[[
(void) ((void)sizeof(char[1 - 2*!!((BOOST_VERSION) < ($1))]));
]])])

# Macro to convert version string (e.g., "1.78.0") to numeric format (e.g., 107800).
# This logic is kept close to the original for compatibility, but uses clearer variable names.
AC_DEFUN([_AX_BOOST_BASE_VERSION_TO_NUMERIC],[
  # Default to 1.20.0 if no version is specified, matching the original macro's logic
  _req_version="$2"
  AS_IF([test "x$_req_version" = "x"], [_req_version="1.20.0"])

  # Use shell parameter expansion/command substitution to extract version parts (less portable than AS_VERSION_COMPARE, but mimics original)
  _req_major=`echo "$_req_version" | cut -d'.' -f1`
  _req_minor=`echo "$_req_version" | cut -d'.' -f2`
  _req_sub_minor=`echo "$_req_version" | cut -d'.' -f3`

  AS_IF([test "x$_req_major" = "x"],
    [AC_MSG_ERROR([You must specify at least the Boost major version])])

  AS_IF([test "x$_req_minor" = "x"], [_req_minor="0"])
  AS_IF([test "x$_req_sub_minor" = "x"], [_req_sub_minor="0"])

  # Calculate numeric version: Major * 100000 + Minor * 100 + Sub_Minor
  _numeric_version=`expr $_req_major \* 100000 \+ $_req_minor \* 100 \+ $_req_sub_minor`
  
  # Set the target variable ($1) to the calculated numeric version
  AS_VAR_SET($1, $_numeric_version)
])

# Main macro definition
AC_DEFUN([AX_BOOST_BASE],
[
  # 1. ARGUMENT PROCESSING: --with-boost
  AC_ARG_WITH([boost],
    [AS_HELP_STRING([--with-boost@<:@=ARG@:>@],
      [use Boost library from a standard location (ARG=yes),
      from the specified location (ARG=<path>),
      or disable it (ARG=no)
      @<:@ARG=yes@:>@ ])],
    [
      AS_CASE([$withval],
        [no],[want_boost="no"; _AX_BOOST_BASE_boost_path=""],
        [yes],[want_boost="yes"; _AX_BOOST_BASE_boost_path=""],
        [want_boost="yes"; _AX_BOOST_BASE_boost_path="$withval"])
    ],
    [want_boost="yes"])

  # 2. ARGUMENT PROCESSING: --with-boost-libdir
  AC_ARG_WITH([boost-libdir],
    [AS_HELP_STRING([--with-boost-libdir=LIB_DIR],
      [Force given directory for Boost libraries.
      Overrides automatic library path detection.])],
    [
      AS_IF([test -d "$withval"],
        [_AX_BOOST_BASE_boost_lib_path="$withval"],
        [AC_MSG_ERROR([--with-boost-libdir expected a valid directory name])])
    ],
    [_AX_BOOST_BASE_boost_lib_path=""])

  # Initialize output variables
  BOOST_LDFLAGS=""
  BOOST_CPPFLAGS=""
  
  # Run detection only if boost is wanted
  AS_IF([test "x$want_boost" = "xyes"],
    [_AX_BOOST_BASE_RUN_DETECTION([$1],[$2],[$3])])

  # Substitute variables for use in Makefiles
  AC_SUBST(BOOST_CPPFLAGS)
  AC_SUBST(BOOST_LDFLAGS)
])

# Run the core detection logic.
AC_DEFUN([_AX_BOOST_BASE_RUN_DETECTION],[
  _AX_BOOST_BASE_VERSION_TO_NUMERIC(REQUIRED_BOOST_VERSION, [$1])
  succeeded="no"
  
  # Determine architecture-specific library subdirectories
  AC_REQUIRE([AC_CANONICAL_HOST])
  AS_CASE([${host_cpu}],
    [x86_64|mips*64*|ppc64|powerpc64|s390x|sparc64|aarch64|ppc64le|powerpc64le|riscv64|e2k],
      [lib_subdirs="lib64 libx32 lib32 lib"],
    [lib_subdirs="lib"]
  )
  
  # Determine multi-arch library subdirectory (e.g., /usr/lib/x86_64-linux-gnu)
  AS_CASE([${host_cpu}],
    [i?86],[multiarch_libsubdir="lib/i386-${host_os}"],
    [armv7l],[multiarch_libsubdir="lib/arm-${host_os}"],
    [multiarch_libsubdir="lib/${host_cpu}-${host_os}"]
  )
  
  search_lib_subdirs="$multiarch_libsubdir $lib_subdirs"
  
  # 3. SEARCH LOGIC - PATH PRIORITY: Explicit Path -> Standard Paths (System/Non-System Layout) -> BOOST_ROOT
  
  # --- Stage 3a: Check user-specified path (--with-boost=PATH) ---
  AS_IF([test "x$_AX_BOOST_BASE_boost_path" != "x"],
    [
      AC_MSG_CHECKING([for Boost includes >= $1 in user path '$_AX_BOOST_BASE_boost_path/include'])
      
      # Check for include directory
      AS_IF([test -d "$_AX_BOOST_BASE_boost_path/include" && test -r "$_AX_BOOST_BASE_boost_path/include"],
        [
          AC_MSG_RESULT([yes])
          BOOST_CPPFLAGS="-I$_AX_BOOST_BASE_boost_path/include"
          
          # Search for lib directory within user path
          for lib_subdir in $search_lib_subdirs; do
            _lib_path_check="$_AX_BOOST_BASE_boost_path/$lib_subdir"
            AC_MSG_CHECKING([for Boost library path in '$_lib_path_check'])
            AS_IF([test -d "$_lib_path_check" && test -r "$_lib_path_check"],
              [
                AC_MSG_RESULT([yes])
                BOOST_LDFLAGS="-L$_lib_path_check"
                break
              ],
              [AC_MSG_RESULT([no])])
          done
        ],
        [AC_MSG_RESULT([no])])
    ],
    [
      # --- Stage 3b: Search Standard Locations (/usr, /usr/local, /opt, etc.) for System Layout ---
      if test X"$cross_compiling" = Xyes; then
        # When cross-compiling, only check multi-arch path first
        _search_paths_for_system_layout=$multiarch_libsubdir
      else
        _search_paths_for_system_layout="$multiarch_libsubdir $lib_subdirs"
      fi

      _found_path_system_layout=""
      for _path in /usr /usr/local /opt /opt/local /opt/homebrew ; do
        if test -d "$_path/include/boost" && test -r "$_path/include/boost" ; then
          for lib_subdir in $_search_paths_for_system_layout ; do
            # Check for existing boost libraries
            if ls "$_path/$lib_subdir/libboost_"* >/dev/null 2>&1 ; then
              _found_path_system_layout="$_path"
              _found_lib_subdir="$lib_subdir"
              break 2 # Found, break both loops
            fi
          done
        fi
      done
      
      if test -n "$_found_path_system_layout" ; then
        BOOST_LDFLAGS="-L$_found_path_system_layout/$_found_lib_subdir"
        BOOST_CPPFLAGS="-I$_found_path_system_layout/include"
      fi
    ])

  # Overwrite library flags if --with-boost-libdir was used
  AS_IF([test "x$_AX_BOOST_BASE_boost_lib_path" != "x"],
    [BOOST_LDFLAGS="-L$_AX_BOOST_BASE_boost_lib_path"])

  # 4. COMPILATION CHECK (Attempt 1: System/User-defined layout)
  AC_MSG_CHECKING([for Boost library >= $1 ($REQUIRED_BOOST_VERSION) using detected flags])
  _CPPFLAGS_SAVED="$CPPFLAGS"
  _LDFLAGS_SAVED="$LDFLAGS"
  
  # Temporarily update compiler flags
  CPPFLAGS="$CPPFLAGS $_CPPFLAGS_SAVED $BOOST_CPPFLAGS"
  LDFLAGS="$LDFLAGS $_LDFLAGS_SAVED $BOOST_LDFLAGS"

  AC_REQUIRE([AC_PROG_CXX])
  AC_LANG_PUSH(C++)
    AC_COMPILE_IFELSE([_AX_BOOST_BASE_PROGRAM($REQUIRED_BOOST_VERSION)],
      [
        AC_MSG_RESULT([yes - using system/explicit path])
        succeeded="yes"
      ],
      [
        AC_MSG_RESULT([no])
      ]
    )
  AC_LANG_POP([C++])

  # 5. FALLBACK SEARCH (Attempt 2: Non-System/Staged Layouts)
  if test "x$succeeded" != "xyes" ; then
    # Reset flags and look for non-system/staged installations (e.g. boost-1_78)
    CPPFLAGS="$_CPPFLAGS_SAVED"
    LDFLAGS="$_LDFLAGS_SAVED"
    BOOST_CPPFLAGS=""
    
    # 5a. Search for versioned or Windows-style includes in user path
    _best_version="0"
    _best_include_path=""
    _best_lib_subdir=""
    
    _search_paths_fallback="$_AX_BOOST_BASE_boost_path $BOOST_ROOT"
    
    for _path in $_search_paths_fallback; do
      if test -d "$_path" && test -r "$_path"; then
        # Check for versioned includes (e.g., /include/boost-1_78)
        for i in `ls -d $_path/include/boost-* 2>/dev/null`; do
          # Extract version number (e.g., 1.78)
          _v_tmp=`echo $i | sed "s#$_path/include/boost-##" | sed 's/_/./'`
          # Compare with best found version
          _v_check=`expr $_v_tmp \> $_best_version`
          if test "x$_v_check" = "x1" ; then
            _best_version=$_v_tmp
            _best_include_path="$_path/include/boost-`echo $_best_version | sed 's/\./_/'`"
            _best_path_for_lib="$_path"
          fi
        done

        # Check for Windows/generic layout (e.g., /boost)
        if test -z "$_best_include_path" ; then
          if test -d "$_path/boost" && test -r "$_path/boost"; then
            _best_include_path="$_path"
            _best_path_for_lib="$_path"
          fi
        fi
        
        # If include path found, determine library path
        if test -n "$_best_include_path" && test -z "$_AX_BOOST_BASE_boost_lib_path"; then
          # Search for lib subdirectory (e.g., stage/lib or lib)
          for lib_subdir in stage/$lib_subdirs $lib_subdirs; do
            if ls "$_best_path_for_lib/$lib_subdir/libboost_"* >/dev/null 2>&1 ; then
              _best_lib_subdir="$lib_subdir"
              break
            fi
          done
        fi
      fi
    done

    # Apply best found non-system paths
    if test -n "$_best_include_path"; then
      AC_MSG_NOTICE([Using non-system Boost includes from '$_best_include_path'.])
      BOOST_CPPFLAGS="-I$_best_include_path"
      
      if test -z "$_AX_BOOST_BASE_boost_lib_path" && test -n "$_best_lib_subdir"; then
        AC_MSG_NOTICE([Using non-system Boost libraries from '$_best_path_for_lib/$_best_lib_subdir'.])
        BOOST_LDFLAGS="-L$_best_path_for_lib/$_best_lib_subdir"
      fi
      
      # Final attempt to compile with non-system flags
      CPPFLAGS="$_CPPFLAGS_SAVED $BOOST_CPPFLAGS"
      LDFLAGS="$_LDFLAGS_SAVED $BOOST_LDFLAGS"
      
      AC_LANG_PUSH(C++)
        AC_COMPILE_IFELSE([_AX_BOOST_BASE_PROGRAM($REQUIRED_BOOST_VERSION)],
          [
            AC_MSG_RESULT([yes - using non-system/staged path])
            succeeded="yes"
          ],
          [
            AC_MSG_RESULT([no])
          ]
        )
      AC_LANG_POP([C++])
    fi
  fi

  # 6. FINAL ACTION & CLEANUP
  if test "x$succeeded" != "xyes" ; then
    AC_MSG_WARN([Could not find Boost library >= $1 ($REQUIRED_BOOST_VERSION). Check http://randspringer.de/boost for more info.])
    # Execute ACTION-IF-NOT-FOUND (if present)
    ifelse([$3], , :, [$3])
  else
    AC_DEFINE(HAVE_BOOST,,[Define if the Boost library is available])
    # Execute ACTION-IF-FOUND (if present)
    ifelse([$2], , :, [$2])
  fi

  # Restore original compiler flags
  CPPFLAGS="$_CPPFLAGS_SAVED"
  LDFLAGS="$_LDFLAGS_SAVED"
])
