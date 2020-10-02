source "$stdenv/setup"
trap 'ev=$?; set +x; exitHandler; exit $ev' EXIT
set -x
set -efuo pipefail

tar -xf "$src"
cd "db-${version}"

: ./depends Preprocessing
sed -i.old s/WinIoCtl.h/winioctl.h/g src/dbinc/win_db.h
sed -i.old s/atomic_init/atomic_init_db/ src/dbinc/atomic.h src/mp/mp_region.c src/mp/mp_mvcc.c src/mp/mp_fget.c src/mutex/mut_method.c src/mutex/mut_tas.c

: ./depends Configuring
cd ./build_unix
../dist/configure --prefix="$out" $configureFlags

: ./depends Building
make $makeFlags

: ./depends Staging and Caching not necessary due to nix.
# Nix approach:
make install
