source "$stdenv/setup"
(
  mkdir -p ./s5 
  cd ./s5 
  cp -r "$s5/ui/default" . 
  chmod -R u+w .
  cd ./default
  patch -p1 < $patch
)
mkdir "$out"
pandoc \
  --self-contained \
  --incremental \
  -t s5 \
  -s "$src" \
  -o "$out/${subname}.html"
