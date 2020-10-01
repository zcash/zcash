source "$stdenv/setup"

tar -xf "$src" --strip-components=1 --one-top-level="."
bash ./install.sh \
  --destdir="$out" \
  --prefix='/' \
  --disable-ldconfig

# Ref: https://nixos.wiki/wiki/Packaging/Binaries#The_Dynamic_Loader
for bin in "$out"/bin/*
do
  if file "$bin" | sed "s,$bin: *,," | grep -q '^ELF'
  then
    basebin="$(basename "$bin")"
    echo "patchelf on ${basebin@Q}."
    patchelf --set-interpreter "$(cat $NIX_CC/nix-support/dynamic-linker)" "$bin"
  fi
done
