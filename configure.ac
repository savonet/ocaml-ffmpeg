AC_INIT([ocaml-ffmpeg],[0.1.2],[savonet-users@lists.sourceforge.net])

VERSION=$PACKAGE_VERSION
AC_MSG_RESULT([configuring $PACKAGE_STRING])

AC_BASE_CHECKS()

PKG_PROG_PKG_CONFIG()
PKG_CONFIG_CHECK_MODULE([libavutil])
PKG_CONFIG_CHECK_MODULE([libswscale])
PKG_CONFIG_CHECK_MODULE([libavformat])
PKG_CONFIG_CHECK_MODULE([libavcodec])
PKG_CONFIG_CHECK_MODULE([libswresample])

# substitutions to perform
AC_SUBST(VERSION)
AC_SUBST(INC)
AC_SUBST(requires)

# Finally create the Makefile and samples
AC_CONFIG_FILES([Makefile],[chmod a-w Makefile])
AC_CONFIG_FILES([src/META])
AC_CONFIG_FILES([src/Makefile])
AC_OUTPUT
