#!/usr/bin/env bash

set -eu



if [ "x$*" = 'x--help' ]
then
    cat <<EOF
Usage:

$0 --help
  Show this help message and exit.

$0 [VERSION]
  Tests the Zcash gitian build against git commit/tag VERSION.
EOF
    exit 0
fi

HOSTS="x86_64-linux-gnu"
CONFIGFLAGS="--enable-glibc-back-compat --enable-reduce-exports --disable-bench --enable-hardening --enable-werror"
MAKEOPTS="V=1"
HOST_CFLAGS=""
HOST_CXXFLAGS=""
HOST_LDFLAGS=-static-libstdc++

export QT_RCC_TEST=0
export GZIP="-1n"
export TZ="UTC"

cd "$(dirname "$(readlink -f "$0")")"

pwd



FAKEGITIAN_DIR=`pwd`/"fake-gitian-environment"

function build_dir_warning {
	echo;echo;echo;echo
	echo "signal received, exiting. to allow for examination, fake-gitian-environment was preserved"
	echo "fake-gitian-environment will be deleted when $0 is run again"
	echo;echo;echo;echo
}

trap build_dir_warning INT
trap build_dir_warning QUIT

echo $FAKEGITIAN_DIR

echo "deleting previous fake-gitian environment"
rm -rf $FAKEGITIAN_DIR
echo "creating fake-gitian environment in zcash/zcutil/fake-gitian-environment"
git clone .. $FAKEGITIAN_DIR
cd $FAKEGITIAN_DIR

export BASEPREFIX=`pwd`/depends

for i in $HOSTS; do
	make ${MAKEOPTS} -j$(nproc) -C ${BASEPREFIX} HOST="${i}"
done

./autogen.sh
CONFIG_SITE=${BASEPREFIX}/`echo "${HOSTS}" | awk '{print $1;}'`/share/config.site ./configure --prefix=/
make dist
SOURCEDIST=`echo zcash-*.tar.gz`
DISTNAME=`echo ${SOURCEDIST} | sed 's/.tar.*//'`
# Correct tar file order
mkdir -p temp
pushd temp
tar xf ../$SOURCEDIST
find zcash* | sort | tar --no-recursion --mode='u+rw,go+r-w,a+X' --owner=0 --group=0 -c -T - | gzip -9n > ../$SOURCEDIST
popd

ORIGPATH="$PATH"
OUTDIR=`pwd`/outdir/
mkdir -p $OUTDIR

# Extract the release tarball into a dir for each host and build
for i in ${HOSTS}; do
	export PATH=${BASEPREFIX}/${i}/native/bin:${ORIGPATH}
	mkdir -p distsrc-${i}
	cd distsrc-${i}
	INSTALLPATH=`pwd`/installed/${DISTNAME}
	mkdir -p ${INSTALLPATH}
	tar --strip-components=1 -xf ../$SOURCEDIST

	CONFIG_SITE=${BASEPREFIX}/${i}/share/config.site ./configure --prefix=/ --disable-ccache --disable-maintainer-mode --disable-dependency-tracking ${CONFIGFLAGS} CFLAGS="${HOST_CFLAGS}" CXXFLAGS="${HOST_CXXFLAGS}" LDFLAGS="${HOST_LDFLAGS}"
	echo "################################################################################"
	echo "################################################################################"
	echo "################################################################################"
	echo "STARTING A HIGHLY PARALLEL make -k -j$(nproc)"
	echo "################################################################################"
	echo "################################################################################"
	echo "################################################################################"
	set +e
	if ! make -j$(nproc) -k ${MAKEOPTS}
	then
		echo "################################################################################"
		echo "################################################################################"
		echo "################################################################################"
		echo "make failed. Running make -k to give you a condensed list of the failures"
		echo "################################################################################"
		echo "################################################################################"
		echo "################################################################################"
		set -e
		make -k ${MAKEOPTS}
		exit 1
	fi
	echo "################################################################################"
	echo "################################################################################"
	echo "################################################################################"
	echo "make succeeded, continuing with remaining build tasks"
	echo "################################################################################"
	echo "################################################################################"
	echo "################################################################################"
	make ${MAKEOPTS} -C src check-security
	make install DESTDIR=${INSTALLPATH}
	cd installed
	find . -name "lib*.la" -delete
	find . -name "lib*.a" -delete
	rm -rf ${DISTNAME}/lib/pkgconfig
	find ${DISTNAME}/bin -type f -executable -exec objcopy --only-keep-debug {} {}.dbg \; -exec strip -s {} \; -exec objcopy --add-gnu-debuglink={}.dbg {} \;
	# Commented out while we don't build any libraries
	#find ${DISTNAME}/lib -type f -exec objcopy --only-keep-debug {} {}.dbg \; -exec strip -s {} \; -exec objcopy --add-gnu-debuglink={}.dbg {} \;
	find ${DISTNAME} -not -name "*.dbg" | sort | tar --no-recursion --mode='u+rw,go+r-w,a+X' --owner=0 --group=0 -c -T - | gzip -9n > ${OUTDIR}/${DISTNAME}-${i}.tar.gz
	find ${DISTNAME} -name "*.dbg" | sort | tar --no-recursion --mode='u+rw,go+r-w,a+X' --owner=0 --group=0 -c -T - | gzip -9n > ${OUTDIR}/${DISTNAME}-${i}-debug.tar.gz
	cd ../../
done

mkdir -p $OUTDIR/src
mv $SOURCEDIST $OUTDIR/src
mv ${OUTDIR}/${DISTNAME}-x86_64-*-debug.tar.gz ${OUTDIR}/${DISTNAME}-linux64-debug.tar.gz
mv ${OUTDIR}/${DISTNAME}-x86_64-*.tar.gz ${OUTDIR}/${DISTNAME}-linux64.tar.gz

echo "it looks like we're done, check out $OUTDIR for the output files :)"
