source "$zcbuildutil"

dst="$out/vendored-sources"
mkdir -p "$dst"

for crate in $crates
do
  tar -xf "$crate" --one-top-level='./crate'
  cratedir="$(ls ./crate)"
  pkgname="$(
    grep ^name "./crate/$cratedir/Cargo.toml" | \
    sed 's/name = "//; s/".*$//'
  )"
  echo "$cratedir -> $dst/$pkgname"
  mv "./crate/$cratedir" "$dst/$pkgname"
done
