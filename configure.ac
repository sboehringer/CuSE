#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.60)
AC_INIT(Cursed Sequencer, 0.5, thomas@haenselmann.de)
AM_INIT_AUTOMAKE(cuse, 0.5)
AC_CONFIG_SRCDIR([src/pool_container.cxx])
AC_CONFIG_FILES([Makefile src/Makefile])
AC_CONFIG_HEADER([config.h])

# Checks for programs.
AC_PROG_CXX
#AC_PROG_CC

# Checks for libraries.
AC_CHECK_LIB([pthread], [pthread_create])
AC_CHECK_LIB([ncurses], [curs_set])
AC_CHECK_LIB([cdk], [getCDKRadioCurrentItem],,[
		    echo 'you need the cdk library >= 5.0 which must contain CDKRadio buttons' 
		    exit 1
		    ])
AC_CHECK_LIB([winmm], [winmmDbgOut])

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([netinet/in.h stdint.h stdlib.h sys/time.h])
CPPFLAGS="$CPPFLAGS -I/usr/include/cdk"
AC_CHECK_HEADERS([cdk/cdk.h],,[
	echo 'cdk version >= 5.0 and includes (developer-version) required'
	exit 1
	])
AC_CHECK_HEADERS([curses.h],,[
	echo 'ncurses version >= 5.2 and includes (developer-version) required'
	exit 1
	])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_CONST
AC_C_INLINE
AC_TYPE_INT16_T
AC_TYPE_INT64_T
AC_HEADER_TIME
AC_TYPE_UINT16_T

# Checks for library functions.
AC_CHECK_FUNCS([atexit gettimeofday])

AC_OUTPUT
