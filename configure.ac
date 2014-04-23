AC_INIT([usbutils],
	[007],
	[linux-usb@vger.kernel.org])
AC_PREREQ(2.60)
AM_INIT_AUTOMAKE([check-news foreign 1.9 dist-bzip2])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])
AC_PREFIX_DEFAULT([/usr])

AC_USE_SYSTEM_EXTENSIONS
AC_SYS_LARGEFILE

AC_CHECK_HEADERS([byteswap.h])
AC_CHECK_FUNCS([nl_langinfo iconv])

PKG_CHECK_MODULES(LIBUSB, libusb-1.0 >= 1.0.0)

AC_CANONICAL_HOST

AC_MSG_CHECKING([operating system])

case $host in
*-linux* | *-uclinux*)
	AC_MSG_RESULT([Linux])
	AC_DEFINE(OS_LINUX, 1, [Linux backend])
	AC_SUBST(OS_LINUX)
	LIBS="-Wl,--as-needed"
	AC_ARG_ENABLE([udev],
		[AC_HELP_STRING([--enable-udev], [use udev for device enumeration (recommended) [default=yes]])],
		[], [enable_udev="yes"])
		if test "x$enable_udev" = "xyes" ; then
			# system has udev. use it or fail!
			PKG_CHECK_MODULES(UDEV, libudev >= 196)
			AC_CHECK_HEADERS([libudev.h],[],[AC_ERROR(["udev support requested but libudev not installed"])])
			AC_CHECK_LIB([udev], [udev_new], [], [AC_ERROR(["udev support requested but libudev not installed"])])
			AC_DEFINE(USE_UDEV, 1, [Use udev for device enumeration/hotplug])
		fi
		AC_SUBST(USE_UDEV)
	;;

*-darwin*)
	AC_MSG_RESULT([Darwin])
	AC_DEFINE(OS_DARWIN, 1, [Darwin backend])
	AC_SUBST(OS_DARWIN)
	LIBS="-Wl,-framework,IOKit -Wl,-framework,CoreFoundation"
	;;
*-openbsd*)
	AC_MSG_RESULT([OpenBSD])
	AC_DEFINE(OS_OPENBSD, 1, [OpenBSD backend])
	AC_SUBST(OS_OPENBSD)
	;;
*-netbsd*)
	AC_MSG_RESULT([NetBSD])
	AC_DEFINE(OS_NETBSD, 1, [NetBSD backend])
	AC_SUBST(OS_NETBSD)
	;;
*-mingw*)
	AC_MSG_RESULT([Windows])
	AC_DEFINE(OS_WINDOWS, 1, [Windows backend])
	AC_SUBST(OS_WINDOWS)
	LIBS=""
	LTLDFLAGS="${LTLDFLAGS} -avoid-version -Wl,--add-stdcall-alias"
	AM_CFLAGS="${AM_CFLAGS} -fno-omit-frame-pointer"
	;;
*-cygwin*)
	AC_MSG_RESULT([Cygwin (using Windows backend)])
	AC_DEFINE(OS_WINDOWS, 1, [Windows backend])
	AC_SUBST(OS_WINDOWS)
	LIBS=""
	LTLDFLAGS="${LTLDFLAGS} -avoid-version -Wl,--add-stdcall-alias"
	;;
*)
	AC_MSG_ERROR([unsupported operating system])
esac

AC_SUBST(LIBS)
AC_SUBST(LTLDFLAGS)
AM_CONDITIONAL(USE_UDEV, test "x$enable_udev" = xyes)

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([
	Makefile
])
AC_CONFIG_SUBDIRS([usbhid-dump])

AC_OUTPUT
AC_MSG_RESULT([
	usbutils $VERSION
	=============

	prefix:                 ${prefix}
	datarootdir:            ${datarootdir}
	datadir:                ${datadir}
	mandir:                 ${mandir}

	usb.ids:                ${datadir}/usb.ids
	zlib:                   ${USE_ZLIB}

	compiler:               ${CC}
	cflags:                 ${CFLAGS}
	ldflags:                ${LDFLAGS}
])
