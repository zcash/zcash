source "$stdenv/setup"
trap 'ev=$?; set +x; exitHandler; exit $ev' EXIT
set -x
set -efuo pipefail

tar -xf "$src"
cd "${pname}-release-${version}-stable"

: ./depends Preprocessing
for patch in $patches
do
  echo "Applying patch: $patch"
  patch -p1 < "$patch"
done

: ./depends Configuring
./autogen.sh
./configure --prefix="$out" $configureOptions

: ./depends Building
make
make install
