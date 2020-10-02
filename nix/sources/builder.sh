source "$stdenv/setup"

mkdir "$out"
for source in $sources
do
  ln -sv "$source" "$out/$(echo "$source" | sed 's/^[^-]*-//')"
done
