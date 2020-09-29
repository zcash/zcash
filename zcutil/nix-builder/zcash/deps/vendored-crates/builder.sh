source "$zcbuildutil"

dst="$out/vendored-sources"
mkdir -p "$dst"

for cratepath in $crates
do
  cratename="$(basename "$cratepath" | sed 's/^[^-]*-//; s/\.crate$//')"
  tar -xf "${cratepath}"
  cat > "${cratename}/.cargo-checksum.json" <<__EOF
    {
      "package": "$(sha256sum "${cratepath}" | sed 's/ .*$//')",
      "files" {}
    }
__EOF
  mv "${cratename}" "${dst}/${cratename}"
  echo "nix-vendored: ${cratename@Q}"
done
