#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.68])
AC_INIT([render_stack], 1.0.0, [https://github.com/MapQuest/MapQuest-Render-Stack/issues])
AM_INIT_AUTOMAKE([foreign])
AC_CONFIG_SRCDIR([.])
AC_CONFIG_HEADERS([config.hpp])
AC_CONFIG_MACRO_DIR([m4])
# AC_CONFIG_AUX_DIR might be helpful...
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

# Checks for programs.
AC_PROG_AWK
AC_PROG_CXX
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_LIBTOOL
AC_PATH_PROG([PROTOC], protoc, [no])
if test "x${PROTOC}" == "xno" ; then 
   echo "Google protocol buffers compiler (protoc) not found."
   exit 1
fi

# Make tests run for C++ compiler
AC_LANG([C++])

# Checks for libraries.
AC_SEARCH_LIBS([BZ2_bzReadOpen], [bz2], [], [AC_MSG_FAILURE([bz2 library files not found, please install libbz2-dev])])
AC_SEARCH_LIBS([gdImageCreateFromJpegPtr], [gd], [], [AC_MSG_FAILURE([gd library files not found, please install libgd2-xpm-dev])])
# FIXME: Replace `CMQExec' with a function in `-lmqclient':
AC_SEARCH_LIBS([CMQExec], [mqclient], [AC_DEFINE([HAVE_MQCLIENT], [1], [Define if you have the MQClient library]),
			  	       have_mqclient=yes], [have_mqclient=no])
AC_SEARCH_LIBS([PQconnectdb], [pq], [AC_DEFINE([HAVE_PQ], [1], [Define if you have the PQ PostgreSQL client library])])
AM_CONDITIONAL([WANT_MAPWARE_TILER], [test "x$have_mqclient" = "xyes"])

# Check pkg-config packaged packages.
PKG_CHECK_MODULES([DEPS], [libmemcached >= 0.49 protobuf >= 2.4.0 libzmq >= 2.1.10 libcurl >= 7.19.5]) 
PKG_CHECK_MODULES([DEPS_PY], [python >= 2.6])

AC_SEARCH_LIBS([uuid_generate], [uuid])

AX_BOOST_BASE([1.45], [], [AC_MSG_FAILURE([Need boost at least version 1.45.])])
AX_BOOST_DATE_TIME 
AX_BOOST_PROGRAM_OPTIONS 
AX_BOOST_IOSTREAMS 
AX_BOOST_SYSTEM 
AX_BOOST_FILESYSTEM
AX_BOOST_THREAD
AX_BOOST_REGEX
AX_BOOST_PYTHON

# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h fcntl.h netdb.h stdint.h string.h sys/socket.h unistd.h])
AC_CHECK_HEADER([mapnik/utils.hpp], [], [AC_MSG_FAILURE([Could not find mapnik headers, please install mapnik.])])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_INT64_T
AC_TYPE_SIZE_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T

# Checks for library functions.
AC_FUNC_ERROR_AT_LINE
AC_FUNC_MALLOC
AC_CHECK_FUNCS([gethostbyname gethostname inet_ntoa memset select socket sqrt strerror])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
