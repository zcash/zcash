source "$stdenv/setup"

tar -xf "$src"
cd "${pname}-release-${version}"
mkdir -p "$out/lib"

function makeinst
{
  echo "making & installing ${2@Q}"
  make -C "$1/make" CXXFLAGS="$CXXFLAGS" "$2"
  cp "$1/make/$2" "$out/lib/lib$2"
  cp -a "$1/include" "$out/"
}

makeinst googlemock gmock.a
makeinst googletest gtest.a
