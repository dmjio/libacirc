AC_INIT([libacirc], [0.1.0], [bcarmer@gmail.com])

AC_CONFIG_HEADERS([src/config.h])

AC_CONFIG_SRCDIR([src/])
AC_CONFIG_AUX_DIR([build/autoconf])
AC_CONFIG_MACRO_DIR([build/autoconf])
AM_INIT_AUTOMAKE([foreign -Wall -Werror subdir-objects])

AM_PROG_AR

LT_INIT

AC_PROG_CC
AC_PROG_INSTALL
dnl AC_PROG_LIBTOOL
AC_PROG_YACC
AC_PROG_LEX

AX_VALGRIND_CHECK

AC_ARG_ENABLE(debug,
[AS_HELP_STRING([--enable-debug], [enable debugging])],
[], [enable_debug=no])

AC_ARG_ENABLE(gmp,
[AS_HELP_STRING([--disable-gmp], [do not compile gmp support])],
[], [enable_gmp=yes])


CFLAGS=                         dnl get rid of default -g -O2
COMMON_CFLAGS="-Wall -Wformat -Wformat-security -Wextra -Wunused \
-Wshadow -Wmissing-prototypes -Wfloat-equal -Wpointer-arith -Wcast-align \
-Wstrict-prototypes -Wredundant-decls -Wendif-labels -Wcast-qual \
-std=gnu11 -Wpedantic"

if test "x$enable_debug" = x"yes"; then
  EXTRA_CFLAGS="-O0 -g"
else
  EXTRA_CFLAGS="-O3"
  AC_DEFINE(NDEBUG,1,[Define whether debugging is enabled])
fi
AC_SUBST(COMMON_CFLAGS)
AC_SUBST(EXTRA_CFLAGS)

AC_CHECK_HEADERS([gmp.h])

if test "x$enable_gmp" = x"yes"; then
  AC_SEARCH_LIBS(__gmpz_init, gmp, [], AC_MSG_ERROR([libgmp not found]))
  AC_DEFINE(HAVE_GMP, 1, [Define whether gmp is enabled])
  AC_SUBST(ACIRC_HAVE_GMP, ["#define HAVE_GMP 1"])
else
  AC_SUBST(ACIRC_HAVE_GMP, [""])
fi

AC_FUNC_MALLOC

AC_CONFIG_FILES([Makefile src/Makefile test/Makefile src/acirc.h])

AC_OUTPUT
