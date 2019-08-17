#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.
AC_PREREQ([2.69])
AC_INIT([buckey], [1.0.0], [tylersengia@gmail.com])
AM_INIT_AUTOMAKE([-Wall -Werror foreign subdir-objects])
AC_CONFIG_SRCDIR([./])
AC_CONFIG_HEADERS([config.h])
AC_PREFIX_DEFAULT([/usr/local])

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC

# Check for pkgconfig
AC_CHECK_PROG(HAVE_PKGCONFIG, pkg-config, yes, no)

# Checks for libraries.
AC_FUNC_STRNLEN

#TODO: Allow the user to specify custom directories to libraries
#TODO: Allow the user to build custom configurations

#Check for yaml-cpp
AC_LANG([C++])
PKG_CHECK_MODULES([YAMLXX],[yaml-cpp], AC_MSG_RESULT([Found yaml-cpp]), AC_MSG_ERROR([Could not find yaml-cpp! Make sure it is installed!]))

#Check for pocketsphinx
PKG_CHECK_MODULES([POCKETSPHINX],[pocketsphinx], AC_MSG_RESULT([Found pocketsphinx]), AC_MSG_ERROR([Could not find pocketsphinx! Make sure it is installed!]))

#Check for sphinxbase
PKG_CHECK_MODULES([SPHINXBASE],[sphinxbase], AC_MSG_RESULT([Found sphinxbase]), AC_MSG_ERROR([Could not find sphinxbase! Make sure it is installed!]))

#Check for mimic
PKG_CHECK_MODULES([MIMIC],[mimic], AC_MSG_RESULT([Found mimic1]), AC_MSG_ERROR([Could not find mimic1! Make sure it is installed!]))

#Check for JSGF Kit++
PKG_CHECK_MODULES([JSGFKITXX],[jsgfkit], AC_MSG_RESULT([Found JSGF Kit++]), AC_MSG_ERROR([Please install JSGF Kit++! See README for additional help.]))

#Check for SDL2
PKG_CHECK_MODULES([SDL2],[sdl2], AC_MSG_RESULT([Found SDL2]), AC_MSG_ERROR([Please install SDL2! See README for additional help.]))

#Check for SDL2_mixer
PKG_CHECK_MODULES([SDL2_mixer],[SDL2_mixer], AC_MSG_RESULT([Found SDL2_mixer]), AC_MSG_ERROR([Please install SDL2_mixer! See README for additional help.]))

#Check for cppfs
AC_CHECK_FILE([/usr/local/include/cppfs/cppfs_api.h], AC_MSG_RESULT([Found cppfs]), AC_MSG_ERROR([Please install cppfs! See README for additional help.]))

# Checks for header files.
AC_CHECK_HEADERS([netdb.h netinet/in.h stdlib.h sys/file.h sys/socket.h syslog.h unistd.h])
AC_HEADER_TIME

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_PID_T
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_FORK
AC_CHECK_FUNCS([memset socket unlink])

### TODO: Make these not hard coded and allow the user to specify library directories.
CPPFS_INCLUDE="/usr/local/include/cppfs"
CPPFS_LD='-lcppfs'

AC_SUBST(SPHINXBASE_CFLAGS)
AC_SUBST(SPHINXBASE_LIBS)

AC_SUBST(POCKETSPHINX_CFLAGS)
AC_SUBST(POCKETSPHINX_LIBS)

AC_SUBST(MIMIC_CFLAGS)
AC_SUBST(MIMIC_LIBS)

AC_SUBST(YAMLXX_CFLAGS)
AC_SUBST(YAMLXX_LIBS)

AC_SUBST(JSGFKITXX_LIBS)
AC_SUBST(JSGFKITXX_CFLAGS)

AC_SUBST(SDL2_CFLAGS)
AC_SUBST(SDL2_LIBS)

AC_SUBST(SDL2_mixer_CFLAGS)
AC_SUBST(SDL2_mixer_CFLAGS)

AC_SUBST(CPPFS_LD)
AC_SUBST(CPPFS_INCLUDE)

AC_OUTPUT([Makefile src/Makefile src/core/Makefile src/tts/Makefile src/filters/Makefile src/sphinx/Makefile])
