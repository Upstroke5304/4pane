dnl Process this file with autoconf to produce a configure script.

AC_INIT([4Pane],[8.0])

AC_PREREQ(2.59)

AC_CONFIG_MACRO_DIRS([.build])
AC_CONFIG_SRCDIR([MyDirs.cpp])


AC_PROG_INSTALL
AC_PROG_LN_S

dnl For some reason, if AC_PROG_CC/CXX find C(XX)FLAGS empty they maliciously fill both with "-g -02"
dnl We append to them in WX_STANDARD_OPTIONS later, causing a confusing duplication or inconsistency in the build output
dnl So cache any user-supplied value and reapply it (or "") after these calls
_CFLAGS=$CFLAGS
_CXXFLAGS=$CXXFLAGS
AC_PROG_CC
AC_PROG_CXX
CFLAGS=$_CFLAGS
CXXFLAGS=$_CXXFLAGS

AC_PROG_CXXCPP

dnl These macros from wxwin.m4 check for a suitable wxWidgets build 
WX_CONFIG_OPTIONS     dnl Supports  --wx-prefix, --wx-exec-prefix, --with-wxdir and --wx-config command line options
WX_STANDARD_OPTIONS([debug,unicode,wxshared,toolkit])
WX_CONVERT_STANDARD_OPTIONS_TO_WXCONFIG_FLAGS
WX_CONFIG_CHECK([3.0.0],[wxWin=1],[wxWin=0],[core,xrc,xml,adv,html])
WX_DETECT_STANDARD_OPTION_VALUES
AS_IF(  [test "$wxWin" = 1],
        [], [AC_MSG_ERROR([
                   wxWidgets must be installed on your system
                   but wx-config script couldn't be found.
                   Please check that wx-config is in your \$PATH
                   and the wxWidgets version is 3.0.0 or above.
                   ])
            ]
     )

AM_INIT_AUTOMAKE([subdir-objects foreign -Wall -Werror])

dnl If building against gtk2 or 3, check the headers are installed
AS_IF(  [test "x$WX_PORT" = xgtk3], [FP_CHECK_GTK_HEADERS(3)],
        [test "x$WX_PORT" = xgtk2], [FP_CHECK_GTK_HEADERS(2)],
        [AC_MSG_ERROR([
            No gtk2 or gtk3 wxWidgets build found in your \$PATH.
            Please install one and try again.
                      ])
        ]
     )

dnl See if we use the system bzip2 headers/lib instead of our built-in versions (the default)
dnl LIBBZ2 gets filled with -lbz2, or nothing if we're using the built-in
FP_BZIP2_CHECK(LIBBZ2)

dnl Check for the xz/lzma headers, without which we can't build the code for xz archive streams
dnl LIBLZMA gets filled with -llzma, or nothing if we're not using xz/lzma
FP_LZMA_ARCHIVE_STREAMS(LIBLZMA)

dnl We need to link to glib-2 for gtk3 builds that use the built-in FSWatcher i.e. those < wx3.0.3 plus wx3.1.0
AS_IF(  [test $WX_VERSION_MAJOR -lt 3], [GLIB2="-lglib-2.0"],
        [test $WX_VERSION_MAJOR$WX_VERSION_MINOR$WX_VERSION_MICRO -lt 303], [GLIB2="-lglib-2.0"],
        [test $WX_VERSION_MAJOR$WX_VERSION_MINOR$WX_VERSION_MICRO = 310],   [GLIB2="-lglib-2.0"],
        [GLIB2=]
     )

dnl link explicitly to required non-wx libs, as needed for non-DT_NEEDED distros
AS_IF(  [test "x$WX_PORT" = x2], [AC_SUBST([XTRLIBS], ["-lgtk-x11-2.0 -lgobject-2.0 -ldl -lglib-2.0 -lgio-2.0 -lutil $LIBBZ2 $LIBLZMA"])],
        [test "x$WX_PORT" = x3], [AC_SUBST([XTRLIBS], ["-lgtk-3 -lgobject-2.0 -ldl $GLIB2 -lgio-2.0 -lutil $LIBBZ2 $LIBLZMA"])],
        [AC_SUBST([XTRLIBS], ["-lutil -ldl $LIBBZ2 $LIBLZMA"])]
     )

FP_CONTROL_ASSERTS
FP_DETECT_GETTEXT

FP_GETARG_ENABLE([install_app], [install_app], [--disable-install_app stops make install installing the app], [enable])
FP_GETARG_ENABLE([symlink], [symlink], [--disable-symlink stops make install creating a 4Pane to 4pane symlink], [enable])
FP_GETARG_ENABLE([desktop], [desktop], [--disable-desktop stops make install trying to create a 4Pane desktop shortcut], [enable])
FP_GETARG_ENABLE([locale], [locale], [--disable-locale stops make install trying to install 4Pane's locale files], [enable])
FP_GETARG_ENABLE([install_rc], [install_rc], [--disable-install_rc stops make install installing the resource files], [enable])
FP_GETARG_ENABLE([install_docs], [install_docs], [--disable-install_docs stops make install installing the manual files], [enable])

dnl Provide an easy way of appending to CXXFLAGS etc, as this is currently hard
dnl It can be used e.g. for the 'hardening' flags when building packages
AC_SUBST([EXTRA_CPPFLAGS])
AC_SUBST([EXTRA_CFLAGS])
AC_SUBST([EXTRA_CXXFLAGS])
AC_SUBST([EXTRA_LDFLAGS])

AC_CONFIG_FILES([Makefile locale/Makefile])

AC_OUTPUT
