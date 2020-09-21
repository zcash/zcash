source "$stdenv/setup"
trap 'ev=$?; set +x; exitHandler; exit $ev' EXIT
set -x
set -efuo pipefail

tar -xf "$src"
cd "${pname}-release-${version}-stable"

: ./depends Preprocessing
for patch in $(ls "$patches")
do
  echo "Applying patch: $patch"
  patch -p1 < "$patches/$patch"
done

: ./depends Configuring
./autogen.sh
./configure --prefix="$out" $configureOptions

: ./depends Building
make
make install
