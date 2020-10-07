source "$stdenv/setup"
set -efuo pipefail
mkdir "$out"
tar -xf "$src"
mv "${pname}-${version}/source" "$out/include"
