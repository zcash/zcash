# ===========================================================================
#     https://www.gnu.org/software/autoconf-archive/ax_boost_thread.html
# ===========================================================================
#
# SYNOPSIS
#
#   AX_BOOST_THREAD
#
# DESCRIPTION
#
#   Test for Thread library from the Boost C++ libraries. The macro requires
#   a preceding call to AX_BOOST_BASE. Further documentation is available at
#   <http://randspringer.de/boost/index.html>.
#
#   This macro calls:
#
#     AC_SUBST(BOOST_THREAD_LIB)
#
#   And sets:
#
#     HAVE_BOOST_THREAD
#
# LICENSE
#
#   Copyright (c) 2009 Thomas Porschberg <thomas@randspringer.de>
#   Copyright (c) 2009 Michael Tindal
#
#   Copying and distribution of this file, with or without modification, are
#   permitted in any medium without royalty provided the copyright notice
#   and this notice are preserved. This file is offered as-is, without any
#   warranty.

#serial 33

AC_DEFUN([AX_BOOST_THREAD],
[
    AC_ARG_WITH([boost-thread],
    AS_HELP_STRING([--with-boost-thread@<:@=special-lib@:>@],
                   [use the Thread library from boost -
                    it is possible to specify a certain library for the linker
                    e.g. --with-boost-thread=boost_thread-gcc-mt ]),
        [
        if test "$withval" = "yes"; then
            want_boost="yes"
            ax_boost_user_thread_lib=""
        else
            want_boost="yes"
            ax_boost_user_thread_lib="$withval"
        fi
        ],
        [want_boost="yes"]
    )

    if test "x$want_boost" = "xyes"; then
        AC_REQUIRE([AC_PROG_CC])
        AC_REQUIRE([AC_CANONICAL_BUILD])
        CPPFLAGS_SAVED="$CPPFLAGS"
        CPPFLAGS="$CPPFLAGS $BOOST_CPPFLAGS"
        export CPPFLAGS

        LDFLAGS_SAVED="$LDFLAGS"
        LDFLAGS="$LDFLAGS $BOOST_LDFLAGS"
        export LDFLAGS

        # MinGW Clang complains about -mthreads as an unused option.
        # See https://github.com/msys2/MINGW-packages/issues/9850
        if echo "$CXX" --version | grep -q clang; then
            CXX_Name_=Clang
        else
            CXX_Name_=NotClang
        fi

        AC_CACHE_CHECK(whether the Boost::Thread library is available,
                       ax_cv_boost_thread,
        [AC_LANG_PUSH([C++])
             CXXFLAGS_SAVE=$CXXFLAGS

             case "x$host_os" in
                 xsolaris )
                     CXXFLAGS="-pthreads $CXXFLAGS"
                     break;
                     ;;
                 xmingw32 )
                     if test "$CXX_Name_" = Clang; then
                         CXXFLAGS="-D_MT $CXXFLAGS"
                     else
                         CXXFLAGS="-mthreads $CXXFLAGS"
                     fi
                     break;
                     ;;
                 *android* )
                     break;
                     ;;
                 * )
                     CXXFLAGS="-pthread $CXXFLAGS"
                     break;
                     ;;
             esac

             AC_COMPILE_IFELSE([
                 AC_LANG_PROGRAM(
                     [[@%:@include <boost/thread/thread.hpp>]],
                     [[boost::thread_group thrds;
                       return 0;]])],
                 ax_cv_boost_thread=yes, ax_cv_boost_thread=no)
             CXXFLAGS=$CXXFLAGS_SAVE
             AC_LANG_POP([C++])
        ])
        if test "x$ax_cv_boost_thread" = "xyes"; then
            case "x$host_os" in
                xsolaris )
                    BOOST_CPPFLAGS="-pthreads $BOOST_CPPFLAGS"
                    break;
                    ;;
                xmingw32 )
                    if test "$CXX_Name_" = Clang; then
                        BOOST_CPPFLAGS="-D_MT $BOOST_CPPFLAGS"
                    else
                        BOOST_CPPFLAGS="-mthreads $BOOST_CPPFLAGS"
                    fi
                    break;
                    ;;
                *android* )
                    break;
                    ;;
                * )
                    BOOST_CPPFLAGS="-pthread $BOOST_CPPFLAGS"
                    break;
                    ;;
            esac

            AC_SUBST(BOOST_CPPFLAGS)

            AC_DEFINE(HAVE_BOOST_THREAD,,
                      [define if the Boost::Thread library is available])
            BOOSTLIBDIR=`echo $BOOST_LDFLAGS | sed -e 's/@<:@^\/@:>@*//'`

            LDFLAGS_SAVE=$LDFLAGS
                        case "x$host_os" in
                          *bsd* )
                               LDFLAGS="-pthread $LDFLAGS"
                          break;
                          ;;
                        esac
            if test "x$ax_boost_user_thread_lib" = "x"; then
                for libextension in `ls -r $BOOSTLIBDIR/libboost_thread* 2>/dev/null | sed 's,.*/lib,,' | sed 's,\..*,,'`; do
                     ax_lib=${libextension}
                    AC_CHECK_LIB($ax_lib, exit,
                                 [link_thread="yes"; break],
                                 [link_thread="no"])
                done
                if test "x$link_thread" != "xyes"; then
                for libextension in `ls -r $BOOSTLIBDIR/boost_thread* 2>/dev/null | sed 's,.*/,,' | sed 's,\..*,,'`; do
                     ax_lib=${libextension}
                    AC_CHECK_LIB($ax_lib, exit,
                                 [link_thread="yes"; break],
                                 [link_thread="no"])
                done
                fi

            else
               for ax_lib in $ax_boost_user_thread_lib boost_thread-$ax_boost_user_thread_lib; do
                      AC_CHECK_LIB($ax_lib, exit,
                                   [link_thread="yes"; break],
                                   [link_thread="no"])
                  done

            fi
            if test "x$ax_lib" = "x"; then
                AC_MSG_ERROR(Could not find a version of the Boost::Thread library!)
            fi
            if test "x$link_thread" = "xno"; then
                AC_MSG_ERROR(Could not link against $ax_lib !)
            else
                BOOST_THREAD_LIB="-l$ax_lib"
                case "x$host_os" in
                    *bsd* )
                        BOOST_LDFLAGS="-pthread $BOOST_LDFLAGS"
                        break;
                        ;;
                    xsolaris )
                        BOOST_THREAD_LIB="$BOOST_THREAD_LIB -lpthread"
                        break;
                        ;;
                    xmingw32 )
                        break;
                        ;;
                    *android* )
                        break;
                        ;;
                    * )
                        BOOST_THREAD_LIB="$BOOST_THREAD_LIB -lpthread"
                        break;
                        ;;
                esac
                AC_SUBST(BOOST_THREAD_LIB)
            fi
        fi

        CPPFLAGS="$CPPFLAGS_SAVED"
        LDFLAGS="$LDFLAGS_SAVED"
    fi
])
