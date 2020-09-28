source "$stdenv/setup"
tar -xf "$src" --strip-components=1 --one-top-level="."
bash ./install.sh \
  --destdir="$out" \
  --prefix='/' \
  --disable-ldconfig
