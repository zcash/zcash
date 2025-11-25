# ===========================================================================
# AX_BOOST_FILESYSTEM: Checks for the Boost Filesystem C++ library.
# ===========================================================================

# serial 28

AC_DEFUN([AX_BOOST_FILESYSTEM],
[
    # Define a unique variable prefix for this macro's internal state.
    AX_BOOST_FILESYSTEM_WANT=yes
    AX_BOOST_FILESYSTEM_USER_LIB=""

    AC_ARG_WITH([boost-filesystem],
        AS_HELP_STRING([--with-boost-filesystem@<:@=special-lib@:>@],
            [Use the Filesystem library from Boost. Optional: specify a linker library name,
             e.g., --with-boost-filesystem=boost_filesystem-gcc-mt]),
        [
            # Handle the user input for --with-boost-filesystem
            AS_CASE(["$withval"],
                [no], [AX_BOOST_FILESYSTEM_WANT=no],
                [yes], [AX_BOOST_FILESYSTEM_WANT=yes],
                [*],
                [
                    AX_BOOST_FILESYSTEM_WANT=yes
                    AX_BOOST_FILESYSTEM_USER_LIB="$withval"
                ]
            )
        ],
        [AX_BOOST_FILESYSTEM_WANT=yes]
    )

    # Proceed with the check only if the user explicitly requested it (or the default is 'yes').
    AS_IF([test "x$AX_BOOST_FILESYSTEM_WANT" = "xyes"], [
        # AC_PROG_CC is required for compilation checks (though AC_LANG_PROGRAM implies C/C++ support).
        AC_REQUIRE([AC_PROG_CC])
        AC_REQUIRE([AX_BOOST_BASE]) # Essential prerequisite check

        # -----------------------------------------------------------
        # Save and set compiler/linker flags provided by AX_BOOST_BASE
        # -----------------------------------------------------------
        CPPFLAGS_SAVED="$CPPFLAGS"
        CPPFLAGS="$CPPFLAGS $BOOST_CPPFLAGS"

        LDFLAGS_SAVED="$LDFLAGS"
        LDFLAGS="$LDFLAGS $BOOST_LDFLAGS"

        LIBS_SAVED="$LIBS"
        # Link against the boost_system library, which is a common dependency for Filesystem.
        LIBS="$LIBS $BOOST_SYSTEM_LIB"
        
        # NOTE: Exporting shell variables is usually not necessary within Autoconf macros 
        # as they operate within the same shell environment, but included for fidelity.
        export CPPFLAGS LDFLAGS LIBS

        # -----------------------------------------------------------
        # Check for header and basic compilation (Boost::Filesystem requires C++ linkage)
        # -----------------------------------------------------------
        AC_CACHE_CHECK([whether the Boost::Filesystem header is available],
                       [ax_cv_boost_filesystem_header],
            [AC_LANG_PUSH([C++])
             AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[@%:@include <boost/filesystem/path.hpp>]],
                                                [[using namespace boost::filesystem;
                                                path my_path( "foo/bar/data.txt" );
                                                return 0;]])],
                               [ax_cv_boost_filesystem_header=yes],
                               [ax_cv_boost_filesystem_header=no])
             AC_LANG_POP([C++])
            ])

        AS_IF([test "x$ax_cv_boost_filesystem_header" = "xyes"], [
            # Header found. Now, attempt to link against the required library.
            AC_DEFINE(HAVE_BOOST_FILESYSTEM,,[define if the Boost::Filesystem library is available])
            
            # Extract the library directory path from BOOST_LDFLAGS (e.g., -L/path/to/boost/lib)
            # This is fragile but necessary for robust Boost library search due to naming conventions.
            BOOSTLIBDIR=`echo $BOOST_LDFLAGS | sed -e 's/-L//'`
            link_filesystem=no
            ax_lib=""

            AS_IF([test "x$AX_BOOST_FILESYSTEM_USER_LIB" = "x"], [
                # --- Default search (no user specified library) ---
                
                # Search for library files using standard naming conventions (libboost_filesystem* and boost_filesystem*).
                # Using 'ls -r' tries the newest/most complex versions first.
                for libextension in `ls -r "$BOOSTLIBDIR"/libboost_filesystem* 2>/dev/null | sed 's,.*/lib,,' | sed 's,\..*,,'` ; do
                    ax_lib=${libextension}
                    AC_CHECK_LIB($ax_lib, exit,
                        [BOOST_FILESYSTEM_LIB="-l$ax_lib"; link_filesystem="yes"; break])
                done
                
                # If libboost_filesystem* failed, try files named boost_filesystem* (sometimes used for non-standard links).
                AS_IF([test "x$link_filesystem" != "xyes"], [
                    for libextension in `ls -r "$BOOSTLIBDIR"/boost_filesystem* 2>/dev/null | sed 's,.*/,,' | sed -e 's,\..*,,'` ; do
                        ax_lib=${libextension}
                        AC_CHECK_LIB($ax_lib, exit,
                            [BOOST_FILESYSTEM_LIB="-l$ax_lib"; link_filesystem="yes"; break])
                    done
                ])
            ], [
                # --- User specified library via --with-boost-filesystem=special-lib ---
                
                # Check the user-provided library name first, then check common variations.
                for ax_lib in "$AX_BOOST_FILESYSTEM_USER_LIB" "boost_filesystem-$AX_BOOST_FILESYSTEM_USER_LIB"; do
                    AC_CHECK_LIB($ax_lib, exit,
                        [BOOST_FILESYSTEM_LIB="-l$ax_lib"; link_filesystem="yes"; break])
                done
            ])

            # -----------------------------------------------------------
            # Final verification and error handling
            # -----------------------------------------------------------
            AS_IF([test "x$link_filesystem" = "xyes"], [
                AC_SUBST(BOOST_FILESYSTEM_LIB) # Success: substitute the found library name.
            ], [
                # Error if we couldn't find a library that links successfully.
                AS_IF([test "x$ax_lib" = "x"], [
                    AC_MSG_ERROR([Could not find a suitable version of the Boost::Filesystem library.])
                ], [
                    AC_MSG_ERROR([Could not link against library '$ax_lib'. Please check your BOOST_LDFLAGS and dependencies.])
                ])
            ])
        ])

        # Restore saved flags to avoid polluting other checks in the configure script.
        CPPFLAGS="$CPPFLAGS_SAVED"
        LDFLAGS="$LDFLAGS_SAVED"
        LIBS="$LIBS_SAVED"
    ])
])
