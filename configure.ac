AC_INIT(libGDSII, 0.1, homer@homerreid.com)
AC_CONFIG_SRCDIR([README])

# Shared-library version.  This has a technical
# meaning related to binary compatibility and is NOT
# the same as the human version number.  See http://tinyurl.com/yegqrlz
SHARED_VERSION_INFO="0:0:0" # CURRENT:REVISION:AGE

AM_INIT_AUTOMAKE
LT_INIT
AM_CONFIG_HEADER([config.h])
AC_CONFIG_MACRO_DIR([m4])
AM_MAINTAINER_MODE
AC_SUBST(SHARED_VERSION_INFO)
AC_DISABLE_SHARED dnl shared libs by default cause too much trouble

AC_PROG_CXX
AC_PROG_INSTALL
AC_LANG([C++])

##################################################
# compiler flags
##################################################
AC_ARG_ENABLE(debug, [AC_HELP_STRING([--enable-debug],[compile for debugging])], enable_debug=$enableval, enable_debug=no)

if test "x$enable_debug" = xyes; then
   CXXFLAGS=-g
   FFLAGS=-g
fi

# add gcc warnings in debug or maintainer mode
if test "x$enable_debug" = xyes || test "$USE_MAINTAINER_MODE" = yes; then
if test "$ac_test_CFLAGS" != "set"; then
   if test $ac_cv_prog_gcc = yes; then
      CXXFLAGS="$CXXFLAGS -Wall -W -Wcast-qual -Wpointer-arith -Wcast-align -pedantic -Wno-long-long -Wshadow -Wwrite-strings -Wredundant-decls -pedantic"
   fi
fi
fi

##################################################
##################################################
##################################################
# Checks for miscellaneous math functions
AC_CHECK_FUNCS([isnan isinf erf erfc copysign])

##################################################
##################################################
##################################################
AC_CONFIG_FILES([
 Makefile
 lib/Makefile
 applications/Makefile
])
AC_OUTPUT

##################################################
##################################################
##################################################
AC_MSG_NOTICE([**])
AC_MSG_NOTICE([** The configure process was successful.])
AC_MSG_NOTICE([** Now type 'make' to compile and sudo 'make install'.])
AC_MSG_NOTICE([**])     
