source "$stdenv/setup"
trap 'ev=$?; set +x; exitHandler; exit $ev' EXIT
set -x
set -efuo pipefail

tar -xf "$src"
cd "${pname}-${version}"
set +x
patchShebangs --build ./Configure
set -x

: ./depends Preprocessing
sed -i.old 's/built on: $$$$date/built on: date not available/' util/mkbuildinf.pl && \
sed -i.old "s|\"engines\", \"apps\", \"test\"|\"engines\"|" Configure

: ./depends Configuring
# What about --openssldir=$out/etc/openssl ?
./Configure --prefix="$out" $configureOptions

: ./depends Building
make install_sw

: ./depends Staging/Caching not necessary due to nix.
rm -rf "$out"/{share,bin,etc}
