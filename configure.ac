#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.
AC_PREREQ([2.69])
AC_INIT([buckey], [1.0.0], [tylersengia@gmail.com])
AM_INIT_AUTOMAKE([-Wall -Werror foreign subdir-objects])
AC_CONFIG_SRCDIR([./])
AC_CONFIG_HEADERS([config.h])
AC_PREFIX_DEFAULT([/usr/local])
#AC_CONFIG_FILES([Makefile])
#AC_CONFIG_FILES([Makefile src/Makefile])

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
AC_LANG_SOURCE([[#include <yaml-cpp/yaml.h>]], [[YAML::Node dummy;]], AC_MGS_RESULT([Found yaml-cpp]), AC_MSG_ERR([Could not find yaml-cpp! Make sure it is installed!]))

#Check for pocketsphinx
AC_CHECK_LIB([pocketsphinx], [ps_init])

#Check for sphinxbase
AC_CHECK_LIB([sphinxbase], [ngram_model_read])

#Check for mimic
AC_CHECK_LIB([mimic], [mimic_init])

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

SPHINX_INCLUDE=`pkg-config --cflags pocketsphinx sphinxbase`
MIMIC_INCLUDE=`pkg-config --cflags mimic`
JSGF_CPP_INCLUDE="/home/tyler/Documents/Programming/Projects/JSGFKit_Plus_Plus/include/"
CPPFS_INCLUDE="/usr/local/include/cppfs"

SPHINXLD=`pkg-config --libs sphinxbase pocketsphinx`
MIMICLD=`pkg-config -libs mimic`
YAMLLD=`pkg-config -libs yaml-cpp`
CPPFSLD='-lcppfs'
JSGFLD="-L/home/tyler/Documents/Programming/Projects/JSGFKit_Plus_Plus/lib/ -lJSGF_Plus_Plus"

AC_SUBST(SPHINXLD)
AC_SUBST(MIMICLD)
AC_SUBST(YAMLLD)
AC_SUBST(CPPFSLD)
AC_SUBST(JSGFLD)
AC_SUBST(JSGF_CPP_INCLUDE)
AC_SUBST(SPHINX_INCLUDE)
AC_SUBST(MIMIC_INCLUDE)
AC_SUBST(CPPFS_INCLUDE)

AC_OUTPUT([Makefile src/Makefile src/core/Makefile src/tts/Makefile src/filters/Makefile src/sphinx/Makefile])
