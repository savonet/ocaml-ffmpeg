AC_INIT([ocaml-ffmpeg],[0.4.0],[savonet-users@lists.sourceforge.net])

VERSION=$PACKAGE_VERSION
requires="bigarray threads"
AC_MSG_RESULT([configuring $PACKAGE_STRING])

AC_BASE_CHECKS()

AC_PATH_PROG(SYSTEM_OCAMLC,"ocamlc")
AC_SUBST(SYSTEM_OCAMLC)

PKG_PROG_PKG_CONFIG()
PKG_CONFIG_CHECK_MODULE([libavutil])
PKG_CONFIG_CHECK_MODULE([libswscale])
PKG_CONFIG_CHECK_MODULE([libavformat])
PKG_CONFIG_CHECK_MODULE([libavcodec])
#PKG_CONFIG_CHECK_MODULE([libswresample])
#PKG_CONFIG_CHECK_MODULE([libavdevice])

# Do we need more?
GEN_CODE_INCLUDE=`$PKG_CONFIG --variable=includedir libavcodec`
AC_SUBST(GEN_CODE_INCLUDE)

AC_CHECK_DECL([avcodec_send_packet],[HAS_AV="yes"],[HAS_AV=""],[#include "libavcodec/avcodec.h"])
AC_SUBST(HAS_AV)

AC_CHECK_HEADERS([libswresample/swresample.h],[HAS_SWRESAMPLE="yes"],[HAS_SWRESAMPLE=""])
if test "x$HAS_SWRESAMPLE" = "xyes" ; then
  LIBS="$LIBS -lswresample"
fi
AC_SUBST(HAS_SWRESAMPLE)


AC_CHECK_HEADERS([libavdevice/avdevice.h],[HAS_AVDEVICE="yes"],[HAS_AVDEVICE=""])
if test "x$HAS_AVDEVICE" = "xyes" ; then
  LIBS="$LIBS -lavdevice"
fi
AC_SUBST(HAS_AVDEVICE)

AC_CHECK_HEADERS([libavutil/channel_layout.h],[HAS_CHANNEL_LAYOUT="yes"],[HAS_CHANNEL_LAYOUT=""])
if test "x$HAS_CHANNEL_LAYOUT" = "xyes" ; then
  CFLAGS="$CFLAGS -DHAS_CHANNEL_LAYOUT"
fi

AC_CHECK_HEADERS([libavutil/frame.h],[HAS_FRAME="yes"],[HAS_FRAME=""])
if test "x$HAS_FRAME" = "xyes" ; then
  CFLAGS="$CFLAGS -DHAS_FRAME"
fi

# substitutions to perform
AC_SUBST(VERSION)
AC_SUBST(INC)
AC_SUBST(requires)

# Finally create the Makefile and samples
AC_CONFIG_FILES([Makefile],[chmod a-w Makefile])
AC_CONFIG_FILES([src/META])
AC_CONFIG_FILES([src/Makefile])
AC_OUTPUT
