pkgdir=$(dirname $0)

cratefile=$(echo "$1" | tr '-' '_')
cratename=$1
cratever=$2
cratehash=$(curl "https://static.crates.io/crates/$cratename/$cratename-$cratever.crate" | sha256sum | awk '{print $1}')

cat "$pkgdir/vendorcrate.mk" |
sed "s/CRATEFILE/$cratefile/g" |
sed "s/CRATENAME/$cratename/g" |
sed "s/CRATEVER/$cratever/g" |
sed "s/CRATEHASH/$cratehash/g" > "$pkgdir/crate_$cratefile.mk"
