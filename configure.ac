#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.63])
AC_INIT(cpuaff,1.0.6,dcdillon@gmail.com,cpuaff)
AM_INIT_AUTOMAKE
AC_OUTPUT(Makefile include/Makefile examples/Makefile tests/Makefile)
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CXX

# Checks for libraries.
# Checks for header files.
AC_CHECK_HEADERS([unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_INT32_T
AC_TYPE_SIZE_T
AC_TYPE_UINT32_T

# cpuaff v2 is Linux-only.
AC_CANONICAL_HOST
case $host in
	*linux*)
		;;
	*)
		AC_MSG_ERROR([cpuaff requires Linux. Use v1.0.6-htaa.1 or earlier for other platforms.])
		;;
esac

CXXFLAGS="$CXXFLAGS -Wall -Werror -pedantic"

AC_OUTPUT
